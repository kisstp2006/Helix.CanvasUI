using System;
using System.Collections.Generic;

namespace HelixEngine.Text
{
    /// <summary>A sprite of a sprite asset: its code point, name and glyph.</summary>
    [Serializable]
    public class SpriteCharacter : TextElement
    {
        string m_Name;

        public string name
        {
            get => m_Name;
            set
            {
                m_Name = value;
                hashCode = TextUtilities.GetHashCode(value ?? "");
            }
        }

        public int hashCode { get; private set; }

        public SpriteCharacter()
        {
            elementType = TextElementType.Sprite;
            unicode = 0xFFFE;
        }

        public SpriteCharacter(uint unicode, SpriteGlyph glyph) : this()
        {
            this.unicode = unicode;
            this.glyph = glyph;
            glyphIndex = glyph?.index ?? 0;
        }

        public SpriteCharacter(uint unicode, SpriteAsset spriteAsset, SpriteGlyph glyph) : this(unicode, glyph)
        {
            textAsset = spriteAsset;
        }

        public SpriteCharacter(uint unicode, uint glyphIndex) : this()
        {
            this.unicode = unicode;
            this.glyphIndex = glyphIndex;
        }
    }

    [Serializable]
    public class SpriteGlyph : Glyph
    {
        public Sprite sprite;

        public SpriteGlyph()
        {
        }

        public SpriteGlyph(uint index, GlyphMetrics metrics, GlyphRect glyphRect, float scale, int atlasIndex) : base(index, metrics, glyphRect, scale, atlasIndex)
        {
        }

        public SpriteGlyph(uint index, GlyphMetrics metrics, GlyphRect glyphRect, float scale, int atlasIndex, Sprite sprite)
            : base(index, metrics, glyphRect, scale, atlasIndex)
        {
            this.sprite = sprite;
        }
    }

    public partial class SpriteAsset
    {
        List<SpriteCharacter> m_Characters;
        List<SpriteGlyph> m_Glyphs;
        Dictionary<uint, SpriteCharacter> m_CharacterLookup;

        public FaceInfo faceInfo
        {
            get
            {
                SpriteFaceData d = FaceInfoInternal();
                return new FaceInfo { pointSize = d.pointSize, scale = d.scale, lineHeight = d.lineHeight, ascentLine = d.ascentLine, baseline = d.baseline, descentLine = d.descentLine };
            }
            set => SetFaceInfoInternal(new SpriteFaceData
            {
                pointSize = value.pointSize, scale = value.scale, lineHeight = value.lineHeight, ascentLine = value.ascentLine, baseline = value.baseline,
                descentLine = value.descentLine,
            });
        }

        public List<SpriteAsset> fallbackSpriteAssets
        {
            get => new List<SpriteAsset>(FallbacksInternal());
            set => SetFallbacksInternal(value?.ToArray() ?? Array.Empty<SpriteAsset>());
        }

        /// <summary>The sprites' characters (changes to the list are applied by UpdateLookupTables).</summary>
        public List<SpriteCharacter> spriteCharacterTable
        {
            get
            {
                if (m_Characters == null)
                    Read();
                return m_Characters;
            }
            set
            {
                m_Characters = value ?? new List<SpriteCharacter>();
                Write();
            }
        }

        public List<SpriteGlyph> spriteGlyphTable
        {
            get
            {
                if (m_Glyphs == null)
                    Read();
                return m_Glyphs;
            }
            set
            {
                m_Glyphs = value ?? new List<SpriteGlyph>();
                Write();
            }
        }

        public Dictionary<uint, SpriteCharacter> spriteCharacterLookupTable
        {
            get
            {
                if (m_CharacterLookup == null)
                    UpdateLookupTables();
                return m_CharacterLookup;
            }
        }

        [Obsolete("The spriteInfoList is now obsolete. Use the spriteCharacterTable and spriteGlyphTable.")]
        public List<SpriteCharacter> spriteInfoList => spriteCharacterTable;

        void Read()
        {
            m_Glyphs = new List<SpriteGlyph>();
            foreach (SpriteGlyphData g in GlyphsInternal())
                m_Glyphs.Add(new SpriteGlyph(g.index, g.metrics, g.glyphRect, g.scale, g.atlasIndex, Resources.EntityIdToObject(g.sprite) as Sprite));
            m_Characters = new List<SpriteCharacter>();
            for (int i = 0; i < CharacterCountInternal(); i++)
            {
                SpriteCharacterData d = CharacterInternal(i);
                SpriteGlyph glyph = m_Glyphs.Find(x => x.index == d.glyphIndex);
                m_Characters.Add(new SpriteCharacter(d.unicode, this, glyph) { name = CharacterNameInternal(i), glyphIndex = d.glyphIndex, scale = d.scale });
            }
        }

        void Write()
        {
            var chars = new SpriteCharacterData[m_Characters?.Count ?? 0];
            for (int i = 0; i < chars.Length; i++)
                chars[i] = new SpriteCharacterData
                {
                    unicode = m_Characters[i].unicode, glyphIndex = m_Characters[i].glyphIndex, scale = m_Characters[i].scale, hashCode = m_Characters[i].hashCode,
                };
            var glyphs = new SpriteGlyphData[m_Glyphs?.Count ?? 0];
            for (int i = 0; i < glyphs.Length; i++)
            {
                SpriteGlyph g = m_Glyphs[i];
                glyphs[i] = new SpriteGlyphData
                {
                    index = g.index, metrics = g.metrics, glyphRect = g.glyphRect, scale = g.scale, atlasIndex = g.atlasIndex,
                    sprite = g.sprite != null ? g.sprite.GetEntityId() : default,
                };
            }
            SetTablesInternal(chars, glyphs);
            for (int i = 0; i < chars.Length; i++)
                SetCharacterNameInternal(i, m_Characters[i].name ?? "");
        }

        /// <summary>The lists to the asset again, and the lookups made anew.</summary>
        public void UpdateLookupTables()
        {
            if (m_Characters != null && m_Glyphs != null)
                Write();
            else
                Read();
            m_CharacterLookup = new Dictionary<uint, SpriteCharacter>();
            foreach (SpriteCharacter c in spriteCharacterTable)
                if (c.unicode != 0xFFFE)
                    m_CharacterLookup[c.unicode] = c;
        }
    }

    /// <summary>A named style: the opening and closing tags it stands for.</summary>
    [Serializable]
    public class TextStyle
    {
        string m_Name;
        string m_OpeningDefinition;
        string m_ClosingDefinition;

        public static TextStyle NormalStyle { get; } = new TextStyle("Normal", "", "");

        public string name
        {
            get => m_Name;
            set
            {
                m_Name = value;
                hashCode = TextUtilities.GetHashCode(value ?? "");
            }
        }

        public int hashCode { get; private set; }
        public string styleOpeningDefinition => m_OpeningDefinition;
        public string styleClosingDefinition => m_ClosingDefinition;
        public uint[] styleOpeningTagArray { get; private set; } = Array.Empty<uint>();
        public uint[] styleClosingTagArray { get; private set; } = Array.Empty<uint>();

        internal TextStyle(string styleName, string styleOpeningDefinition, string styleClosingDefinition)
        {
            name = styleName;
            m_OpeningDefinition = styleOpeningDefinition ?? "";
            m_ClosingDefinition = styleClosingDefinition ?? "";
            RefreshStyle();
        }

        public void RefreshStyle()
        {
            styleOpeningTagArray = Codes(m_OpeningDefinition);
            styleClosingTagArray = Codes(m_ClosingDefinition);
        }

        static uint[] Codes(string s)
        {
            var codes = new uint[s.Length];
            for (int i = 0; i < s.Length; i++)
                codes[i] = s[i];
            return codes;
        }
    }

    public partial class TextStyleSheet
    {
        /// <summary>The sheet's styles (changes to the list are applied by RefreshStyles).</summary>
        public List<TextStyle> styles
        {
            get
            {
                var list = new List<TextStyle>();
                for (int i = 0; i < StyleCountInternal(); i++)
                    list.Add(new TextStyle(StyleNameInternal(i), StyleOpeningInternal(i), StyleClosingInternal(i)));
                return list;
            }
        }

        public TextStyle GetStyle(int hashCode)
        {
            int i = FindStyleInternal(hashCode);
            return i >= 0 ? new TextStyle(StyleNameInternal(i), StyleOpeningInternal(i), StyleClosingInternal(i)) : null;
        }

        public TextStyle GetStyle(string name) => GetStyle(TextUtilities.GetHashCode(name ?? ""));

        /// <summary>A style added, or changed when the sheet has one of its name.</summary>
        public void AddStyle(string name, string openingDefinition, string closingDefinition) => SetStyleInternal(name, openingDefinition ?? "", closingDefinition ?? "");
    }

    public partial class TextColorGradient
    {
        public static TextColorGradient CreateInstance(Color color) => Make(ColorMode.Single, color, color, color, color);

        public static TextColorGradient CreateInstance(Color color0, Color color1, Color color2, Color color3) => Make(ColorMode.FourCornersGradient, color0, color1, color2, color3);

        static TextColorGradient Make(ColorMode mode, Color tl, Color tr, Color bl, Color br)
        {
            var g = CreateInstance<TextColorGradient>();
            g.colorMode = mode;
            g.topLeft = tl;
            g.topRight = tr;
            g.bottomLeft = bl;
            g.bottomRight = br;
            return g;
        }
    }

    public partial class TextSettings
    {
        public static List<FontAsset> fallbackFontAssets
        {
            get => new List<FontAsset>(FallbackFontAssetsInternal());
            set => SetFallbackFontAssetsInternal(value?.ToArray() ?? Array.Empty<FontAsset>());
        }

        public static List<Object> emojiFallbackTextAssets
        {
            get => new List<Object>(EmojiFallbackTextAssetsInternal());
            set => SetEmojiFallbackTextAssetsInternal(value?.ToArray() ?? Array.Empty<Object>());
        }

        public static List<string> fontFeatures { get; set; } = new List<string> { "kern" };
        public static LineBreakingTable linebreakingRules { get; } = new LineBreakingTable();
        public static Dictionary<int, char> leadingCharacters => linebreakingRules.leadingCharacters;
        public static Dictionary<int, char> followingCharacters => linebreakingRules.followingCharacters;

        /// <summary>The characters a line may not start or end with (the line break rules of the UI's text decide them).</summary>
        public class LineBreakingTable
        {
            public Dictionary<int, char> leadingCharacters = new Dictionary<int, char>();
            public Dictionary<int, char> followingCharacters = new Dictionary<int, char>();
        }
    }

    /// <summary>The distance field shader's property ids, keywords and the material's padding.</summary>
    public static class ShaderUtilities
    {
        public static int ID_MainTex = Shader.PropertyToID("_MainTex");
        public static int ID_FaceTex = Shader.PropertyToID("_FaceTex");
        public static int ID_FaceColor = Shader.PropertyToID("_FaceColor");
        public static int ID_FaceDilate = Shader.PropertyToID("_FaceDilate");
        public static int ID_Shininess = Shader.PropertyToID("_FaceShininess");
        public static int ID_OutlineTex = Shader.PropertyToID("_OutlineTex");
        public static int ID_OutlineColor = Shader.PropertyToID("_OutlineColor");
        public static int ID_OutlineWidth = Shader.PropertyToID("_OutlineWidth");
        public static int ID_OutlineSoftness = Shader.PropertyToID("_OutlineSoftness");
        public static int ID_UnderlayColor = Shader.PropertyToID("_UnderlayColor");
        public static int ID_UnderlayOffsetX = Shader.PropertyToID("_UnderlayOffsetX");
        public static int ID_UnderlayOffsetY = Shader.PropertyToID("_UnderlayOffsetY");
        public static int ID_UnderlayDilate = Shader.PropertyToID("_UnderlayDilate");
        public static int ID_UnderlaySoftness = Shader.PropertyToID("_UnderlaySoftness");
        public static int ID_WeightNormal = Shader.PropertyToID("_WeightNormal");
        public static int ID_WeightBold = Shader.PropertyToID("_WeightBold");
        public static int ID_GradientScale = Shader.PropertyToID("_GradientScale");
        public static int ID_ScaleRatio_A = Shader.PropertyToID("_ScaleRatioA");
        public static int ID_ScaleRatio_B = Shader.PropertyToID("_ScaleRatioB");
        public static int ID_ScaleRatio_C = Shader.PropertyToID("_ScaleRatioC");
        public static int ID_TextureWidth = Shader.PropertyToID("_TextureWidth");
        public static int ID_TextureHeight = Shader.PropertyToID("_TextureHeight");
        public static int ID_ClipRect = Shader.PropertyToID("_ClipRect");
        public static int ID_MaskSoftnessX = Shader.PropertyToID("_MaskSoftnessX");
        public static int ID_MaskSoftnessY = Shader.PropertyToID("_MaskSoftnessY");
        public static int ID_StencilID = Shader.PropertyToID("_Stencil");
        public static int ID_StencilOp = Shader.PropertyToID("_StencilOp");
        public static int ID_StencilComp = Shader.PropertyToID("_StencilComp");
        public static int ID_StencilReadMask = Shader.PropertyToID("_StencilReadMask");
        public static int ID_StencilWriteMask = Shader.PropertyToID("_StencilWriteMask");
        public static int ID_ShaderFlags = Shader.PropertyToID("_ShaderFlags");
        public static int ID_ScaleX = Shader.PropertyToID("_ScaleX");
        public static int ID_ScaleY = Shader.PropertyToID("_ScaleY");
        public static int ID_PerspectiveFilter = Shader.PropertyToID("_PerspectiveFilter");
        public static int ID_Sharpness = Shader.PropertyToID("_Sharpness");

        public static string Keyword_Bevel = "BEVEL_ON";
        public static string Keyword_Glow = "GLOW_ON";
        public static string Keyword_Underlay = "UNDERLAY_ON";
        public static string Keyword_Ratios = "RATIOS_OFF";
        public static string Keyword_MASK_SOFT = "MASK_SOFT";
        public static string Keyword_MASK_HARD = "MASK_HARD";
        public static string Keyword_MASK_TEX = "MASK_TEX";
        public static string Keyword_Outline = "OUTLINE_ON";
        public static string ShaderTag_ZTestMode = "helix_GUIZTestMode";
        public static string ShaderTag_CullMode = "_CullMode";

        public static Shader ShaderRef_MobileSDF => Shader.Find("Text/Distance Field");
        public static Shader ShaderRef_MobileBitmap => Shader.Find("UI/Default");

        public static void GetShaderPropertyIDs()
        {
        }

        public static void UpdateShaderRatios(Material mat)
        {
            if (mat != null && !mat.HasProperty(ID_ScaleRatio_A))
                return;
        }

        /// <summary>How far past a glyph's rectangle the material draws (outline, dilate, underlay), in atlas pixels.</summary>
        public static float GetPadding(Material material, bool enableExtraPadding, bool isBold)
        {
            if (material == null || !material.HasProperty(ID_GradientScale))
                return enableExtraPadding ? 4 : 0;
            float gradient = material.GetFloat(ID_GradientScale);
            float dilate = Mathf.Max(material.GetFloat(ID_FaceDilate), 0);
            float outline = material.GetFloat(ID_OutlineWidth) * 0.5f;
            float weight = (isBold ? material.GetFloat(ID_WeightBold) : material.GetFloat(ID_WeightNormal)) / 4;
            float under = material.IsKeywordEnabled(Keyword_Underlay)
                ? Mathf.Max(Mathf.Abs(material.GetFloat(ID_UnderlayOffsetX)), Mathf.Abs(material.GetFloat(ID_UnderlayOffsetY))) + material.GetFloat(ID_UnderlayDilate)
                : 0;
            float padding = Mathf.Max(dilate + outline + weight, under) * gradient;
            return padding + (enableExtraPadding ? 4 : 0);
        }

        public static float GetPadding(Material[] materials, bool enableExtraPadding, bool isBold)
        {
            float max = 0;
            foreach (Material m in materials ?? Array.Empty<Material>())
                max = Mathf.Max(max, GetPadding(m, enableExtraPadding, isBold));
            return max;
        }

        public static bool IsMaskingEnabled(Material material) =>
            material != null && (material.IsKeywordEnabled(Keyword_MASK_SOFT) || material.IsKeywordEnabled(Keyword_MASK_HARD) || material.IsKeywordEnabled(Keyword_MASK_TEX));

        public static Vector4 GetFontExtent(Material material) => Vector4.zero;
    }

    /// <summary>Characters of font assets and sprites of sprite assets, with their fallbacks.</summary>
    public class FontAssetUtilities
    {
        public static FontAssetUtilities instance { get; } = new FontAssetUtilities();

        public static Character GetCharacterFromFontAsset(uint unicode, FontAsset sourceFontAsset, bool includeFallbacks, FontStyles fontStyle, FontWeight fontWeight,
                                                         out bool isAlternativeTypeface)
        {
            isAlternativeTypeface = false;
            if (sourceFontAsset == null)
                return null;
            if (sourceFontAsset.HasCharacter(unicode, false, true) && sourceFontAsset.characterLookupTable.TryGetValue(unicode, out Character c))
                return c;
            if (!includeFallbacks)
                return null;
            foreach (FontAsset f in sourceFontAsset.fallbackFontAssetTable)
            {
                Character found = GetCharacterFromFontAsset(unicode, f, true, fontStyle, fontWeight, out isAlternativeTypeface);
                if (found != null)
                    return found;
            }
            return null;
        }

        public static Character GetCharacterFromFontAssets(uint unicode, FontAsset sourceFontAsset, List<FontAsset> fontAssets, bool includeFallbacks,
                                                          FontStyles fontStyle, FontWeight fontWeight, out bool isAlternativeTypeface)
        {
            Character c = GetCharacterFromFontAsset(unicode, sourceFontAsset, includeFallbacks, fontStyle, fontWeight, out isAlternativeTypeface);
            if (c != null)
                return c;
            foreach (FontAsset f in fontAssets ?? new List<FontAsset>())
            {
                c = GetCharacterFromFontAsset(unicode, f, includeFallbacks, fontStyle, fontWeight, out isAlternativeTypeface);
                if (c != null)
                    return c;
            }
            return null;
        }

        public static SpriteCharacter GetSpriteCharacterFromSpriteAsset(uint unicode, SpriteAsset spriteAsset, bool includeFallbacks)
        {
            SpriteAsset holder = SpriteAsset.SearchForSpriteByUnicode(spriteAsset, unicode, includeFallbacks, out int index);
            return holder != null && index >= 0 ? holder.spriteCharacterTable[index] : null;
        }
    }

    public abstract partial class TextBase
    {
        /// <summary>The style of the whole text (its opening and closing tags around the text).</summary>
        public TextStyle textStyle
        {
            get
            {
                TextStyleSheet sheet = styleSheet != null ? styleSheet : TextSettings.defaultStyleSheet;
                return sheet != null ? sheet.GetStyle(textStyleHashCode) ?? TextStyle.NormalStyle : TextStyle.NormalStyle;
            }
            set => textStyleHashCode = value != null ? value.hashCode : TextStyle.NormalStyle.hashCode;
        }

        public List<string> fontFeatures { get; set; } = new List<string>(TextSettings.fontFeatures);

        public ITextPreprocessor textPreprocessor { get; set; }

        static Func<int, string, FontAsset> s_OnFontAssetRequest;
        static Func<int, string, SpriteAsset> s_OnSpriteAssetRequest;
        static bool s_RequestsHooked;

        static void HookRequests()
        {
            if (s_RequestsHooked)
                return;
            s_RequestsHooked = true;
            fontAssetRequest += hash =>
            {
                string name = RequestNameInternal();
                foreach (Func<int, string, FontAsset> f in s_OnFontAssetRequest?.GetInvocationList() ?? Array.Empty<Delegate>())
                {
                    FontAsset found = f(hash, name);
                    if (found != null)
                    {
                        SetRequestedFontInternal(found);
                        return;
                    }
                }
            };
            spriteAssetRequest += hash =>
            {
                string name = RequestNameInternal();
                foreach (Func<int, string, SpriteAsset> f in s_OnSpriteAssetRequest?.GetInvocationList() ?? Array.Empty<Delegate>())
                {
                    SpriteAsset found = f(hash, name);
                    if (found != null)
                    {
                        SetRequestedSpriteInternal(found);
                        return;
                    }
                }
            };
        }

        /// <summary>A font a tag names that the registry and the settings' path do not have.</summary>
        public static event Func<int, string, FontAsset> OnFontAssetRequest
        {
            add
            {
                HookRequests();
                s_OnFontAssetRequest += value;
            }
            remove => s_OnFontAssetRequest -= value;
        }

        public static event Func<int, string, SpriteAsset> OnSpriteAssetRequest
        {
            add
            {
                HookRequests();
                s_OnSpriteAssetRequest += value;
            }
            remove => s_OnSpriteAssetRequest -= value;
        }

        /// <summary>A character no font of the text has (raised as the text is laid out).</summary>
        public static event MissingCharacterEventCallback OnMissingCharacter;

        internal static void RaiseMissingCharacter(int unicode, int stringIndex, string text, FontAsset fontAsset, TextBase textComponent) =>
            OnMissingCharacter?.Invoke(unicode, stringIndex, text, fontAsset, textComponent);

        public struct SpecialCharacter
        {
            public Character character;
            public FontAsset fontAsset;
            public Material material;
            public int materialIndex;

            public SpecialCharacter(Character character, int materialIndex)
            {
                this.character = character;
                fontAsset = character.textAsset as FontAsset;
                material = fontAsset != null ? fontAsset.material : null;
                this.materialIndex = materialIndex;
            }
        }
    }

    /// <summary>Rewrites a text before it is parsed.</summary>
    public interface ITextPreprocessor
    {
        string PreprocessText(string text);
    }
}
