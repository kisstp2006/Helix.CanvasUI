using HelixEngine.Builtin;
using HelixEngine.Shaders;
using static HelixEngine.Shaders.ShaderMath;

namespace HelixEngine.CanvasUI.Shaders;

/// <summary>The text material's values and the canvas's per batch ones (the same names as UI/Default's).</summary>
public struct TextMaterialData
{
    public float4 _FaceColor;
    public float4 _OutlineColor;
    public float4 _UnderlayColor;
    public float4 _ClipRect;
    public float4 _TextureSampleAdd;
    public float _FaceDilate;
    public float _OutlineWidth;
    public float _OutlineSoftness;
    public float _UnderlayOffsetX;
    public float _UnderlayOffsetY;
    public float _UnderlayDilate;
    public float _UnderlaySoftness;
    public float _WeightNormal;
    public float _WeightBold;
    public float _GradientScale;
    public float _TextureWidth;
    public float _TextureHeight;
    public float _UIMaskSoftnessX;
    public float _UIMaskSoftnessY;
    public float _UIVertexToLinear;
    public float _ScaleRatioA;
}

public struct TextVaryings
{
    [Position] public float4 position;
    [Location(0)] public float4 color;
    /// <summary>xy: the atlas's uv; z: the glyph's kind (0 a distance field, 1 a multi channel one, 2 coverage); w: screen
    /// pixels per distance unit (negative for bold).</summary>
    [Location(1)] public float4 uv;
    /// <summary>The point in the root canvas's space (the batch's clip is there).</summary>
    [Location(2)] public float2 canvasPosition;
}

/// <summary>
/// Text/Distance Field: the text's glyphs from their distance fields, sharp at every size and scale. The face's edge
/// moves with the dilate and the weight (bold), an outline of a width (0 to 1 of the field's range) and softness
/// straddles it, an underlay (UNDERLAY_ON) of an offset, dilate and softness lies under the glyph. The vertex's w is
/// the antialiasing scale (screen pixels per unit of distance; the text sets it for its size and scale). Multi channel
/// fields take the median of their channels; coverage glyphs draw their coverage. Clipped to the batch's rectangle,
/// premultiplied, stencil and color mask from the properties as UI/Default's.
/// </summary>
[Shader("Text/Distance Field")]
[Tag("Queue", "Transparent")]
[Tag("IgnoreProjector", "True")]
[Tag("RenderType", "Transparent")]
public sealed class DistanceField : ShaderDefinition
{
    [Property("Font Atlas"), MainTexture, DefaultTexture("white"), PerRendererData] public Texture2D<float4> _MainTex;
    [Property("Face Color"), Default(1.0f, 1.0f, 1.0f, 1.0f)] public float4 _FaceColor;
    [Property("Face Dilate"), Default(0.0f)] public float _FaceDilate;
    [Property("Outline Color"), Default(0.0f, 0.0f, 0.0f, 1.0f)] public float4 _OutlineColor;
    [Property("Outline Thickness"), Default(0.0f)] public float _OutlineWidth;
    [Property("Outline Softness"), Default(0.0f)] public float _OutlineSoftness;
    [Property("Underlay Color"), Default(0.0f, 0.0f, 0.0f, 0.5f)] public float4 _UnderlayColor;
    [Property("Underlay Offset X"), Default(0.0f)] public float _UnderlayOffsetX;
    [Property("Underlay Offset Y"), Default(0.0f)] public float _UnderlayOffsetY;
    [Property("Underlay Dilate"), Default(0.0f)] public float _UnderlayDilate;
    [Property("Underlay Softness"), Default(0.0f)] public float _UnderlaySoftness;
    [Property("Weight Normal"), Default(0.0f)] public float _WeightNormal;
    [Property("Weight Bold"), Default(0.5f)] public float _WeightBold;
    [Property("Gradient Scale"), Default(9.0f)] public float _GradientScale;
    [Property("Texture Width"), Default(1024.0f)] public float _TextureWidth;
    [Property("Texture Height"), Default(1024.0f)] public float _TextureHeight;
    [Property("Scale Ratio A"), Default(1.0f)] public float _ScaleRatioA;
    // Outside a canvas (a world text) nothing clips: the canvas's batches set their rectangle.
    [Property("Clip Rect"), Default(-32767.0f, -32767.0f, 32767.0f, 32767.0f)] public float4 _ClipRect;
    [Property("Stencil Comparison"), Default(8.0f)] public float _StencilComp;
    [Property("Stencil ID"), Default(0.0f)] public float _Stencil;
    [Property("Stencil Operation"), Default(0.0f)] public float _StencilOp;
    [Property("Stencil Write Mask"), Default(255.0f)] public float _StencilWriteMask;
    [Property("Stencil Read Mask"), Default(255.0f)] public float _StencilReadMask;
    [Property("Color Mask"), Default(15.0f)] public float _ColorMask;
    [Property("Depth Test"), HideInInspector, Default(4.0f)] public float helix_GUIZTestMode;

    [Keyword(KeywordScope.Local)] public static readonly bool UNDERLAY_ON;
    [Keyword(KeywordScope.Local)] public static readonly bool HELIX_UI_ALPHACLIP;

    [Pass("Default", Cull = CullMode.Off, ZWrite = false, ZTestProperty = "helix_GUIZTestMode", ColorMaskProperty = "_ColorMask")]
    [Blend(BlendFactor.One, BlendFactor.OneMinusSrcAlpha)]
    [Stencil(ReferenceProperty = "_Stencil", ReadMaskProperty = "_StencilReadMask", WriteMaskProperty = "_StencilWriteMask", CompareProperty = "_StencilComp",
             PassProperty = "_StencilOp")]
    public static class Default
    {
        [Uniform(0)] public static TextMaterialData material;
        [Texture(0)] public static Texture2D<float4> _MainTex;
        [Sampler(0)] public static SamplerState sampler_MainTex;

        static float ToLinear(float c) => c <= 0.04045f ? c / 12.92f : Pow((c + 0.055f) / 1.055f, 2.4f);

        [VertexShader]
        public static TextVaryings Vertex([Location(VertexInputs.Position)] float3 position, [Location(VertexInputs.Color)] float4 color,
                                          [Location(VertexInputs.TexCoord0)] float4 uv)
        {
            TextVaryings o;
            o.position = ShaderVariables.ObjectToClip(position);
            float4 c = color;
            if (material._UIVertexToLinear > 0.5f)
                c = new float4(ToLinear(c.x), ToLinear(c.y), ToLinear(c.z), c.w);
            o.color = c;
            o.uv = uv;
            o.canvasPosition = position.xy;
            return o;
        }

        // The distance at a point of the atlas (the median of a multi channel field's channels).
        static float Distance(float2 uv, float kind)
        {
            float4 t = _MainTex.Sample(sampler_MainTex, uv);
            if (kind > 0.5f && kind < 1.5f)
                return Max(Min(t.x, t.y), Min(Max(t.x, t.y), t.z));
            return t.x;
        }

        static float Keep(float p, float lo, float hi, float softness)
        {
            float inside = Min(p - lo, hi - p);
            return softness > 0.0f ? Saturate(inside / softness) : (inside >= 0.0f ? 1.0f : 0.0f);
        }

        [FragmentShader]
        [return: Target(0)]
        public static float4 Fragment(TextVaryings input)
        {
            TextMaterialData m = material;
            float kind = input.uv.z;
            float scale = Abs(input.uv.w);
            float d = Distance(input.uv.xy, kind);
            float4 face = new float4(m._FaceColor.x * input.color.x, m._FaceColor.y * input.color.y, m._FaceColor.z * input.color.z, m._FaceColor.w * input.color.w);
            float3 rgb;
            float alpha;
            if (kind > 1.5f || scale <= 0.0f)
            {
                // Coverage: the glyph's coverage as written.
                rgb = face.xyz;
                alpha = d * face.w;
            }
            else
            {
                // The face's edge moved by the dilate and the weight; the outline straddles it.
                float weight = (input.uv.w < 0.0f ? m._WeightBold : m._WeightNormal) * 0.25f + m._FaceDilate;
                float edge = 0.5f - weight * 0.5f * m._ScaleRatioA;
                float width = m._OutlineWidth * 0.5f * m._ScaleRatioA;
                float s = scale / (1.0f + m._OutlineSoftness * scale);
                float all = Saturate((d - (edge - width * 0.5f)) * s + 0.5f);
                float inner = width > 0.0f ? Saturate((d - (edge + width * 0.5f)) * s + 0.5f) : 1.0f;
                float4 outline = new float4(m._OutlineColor.x, m._OutlineColor.y, m._OutlineColor.z, m._OutlineColor.w * input.color.w);
                float4 color = Lerp(outline, face, inner);
                rgb = color.xyz;
                alpha = color.w * all;
            }
            float3 premultiplied = rgb * alpha;
            if (UNDERLAY_ON)
            {
                // The atlas's v runs down: a positive y offset moves the underlay up.
                float2 offset = new float2(m._UnderlayOffsetX * m._GradientScale / m._TextureWidth, -m._UnderlayOffsetY * m._GradientScale / m._TextureHeight);
                float under = Distance(input.uv.xy - offset, kind);
                float ue = 0.5f - m._UnderlayDilate * 0.5f * m._ScaleRatioA;
                float us = scale / (1.0f + m._UnderlaySoftness * scale);
                float ua = Saturate((under - ue) * us + 0.5f) * m._UnderlayColor.w * input.color.w * (1.0f - alpha);
                premultiplied = premultiplied + m._UnderlayColor.xyz * ua;
                alpha = alpha + ua;
            }
            float4 r = m._ClipRect;
            float keep = Keep(input.canvasPosition.x, r.x, r.z, m._UIMaskSoftnessX) * Keep(input.canvasPosition.y, r.y, r.w, m._UIMaskSoftnessY);
            if (HELIX_UI_ALPHACLIP && alpha * keep < 0.001f)
                Discard();
            return new float4(premultiplied * keep, alpha * keep);
        }
    }
}
