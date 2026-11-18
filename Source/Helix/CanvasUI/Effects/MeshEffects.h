#pragma once

#include <Helix/CanvasUI/Rendering/Graphic.h>

// The mesh effects: components that change the mesh of the graphic of their GameObject after it is built. Shadow adds
// an offset copy in the effect color under the vertices, Outline four of them, PositionAsUV1 puts each vertex's
// position into its second texture coordinates.
namespace helix::canvasui
{
    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Abstract)
    class BaseMeshEffect : public UIBehaviour, public IMeshModifier
    {
    public:
        BaseMeshEffect() = default;
        ~BaseMeshEffect() override = default;

        HX_PROPERTY(ReadOnly, Protected) Graphic* graphic() const;
        HX_FUNCTION() void ModifyMesh(VertexHelper* vh) override = 0;
        // The mesh through a vertex helper (the reference's older form).
        HX_FUNCTION(Name = "ModifyMesh", Attributes = "[System.Obsolete(\"use BaseMeshEffect.ModifyMeshes instead\", false)]") virtual void ModifyMeshOf(engine::Mesh* mesh);

        IMeshModifier* AsMeshModifier() override { return this; }
        void OnEnable() override;
        void OnDisable() override;
        void OnDidApplyAnimationProperties() override;

    protected:
        void Dirty();
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class Shadow : public BaseMeshEffect
    {
    public:
        Shadow() = default;
        ~Shadow() override = default;

        HX_PROPERTY() Color effectColor() const { return effectColor_; }
        void SetEffectColor(Color value);
        // Clamped to 600 units each way.
        HX_PROPERTY() Vector2 effectDistance() const { return effectDistance_; }
        void SetEffectDistance(Vector2 value);
        HX_PROPERTY() bool useGraphicAlpha() const { return useGraphicAlpha_; }
        void SetUseGraphicAlpha(bool value);
        HX_FUNCTION() void ModifyMesh(VertexHelper* vh) override;

        // The vertices from start to end become the shadow (moved by x, y, in the color), their copies appended.
        void ApplyShadowTo(Array<engine::UIVertex>& verts, Color32 color, i32 start, i32 end, f32 x, f32 y) const;

    private:
        HX_FIELD(SerializeField, Name = "m_EffectColor") Color effectColor_ = Color(0, 0, 0, 0.5f);
        HX_FIELD(SerializeField, Name = "m_EffectDistance") Vector2 effectDistance_ = Vector2(1, -1);
        HX_FIELD(SerializeField, Name = "m_UseGraphicAlpha") bool useGraphicAlpha_ = true;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class Outline : public Shadow
    {
    public:
        Outline() = default;
        ~Outline() override = default;
        HX_FUNCTION() void ModifyMesh(VertexHelper* vh) override;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class PositionAsUV1 : public BaseMeshEffect
    {
    public:
        PositionAsUV1() = default;
        ~PositionAsUV1() override = default;
        HX_FUNCTION() void ModifyMesh(VertexHelper* vh) override;
    };
}
