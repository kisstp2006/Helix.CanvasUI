#pragma once

#include <Helix/CanvasUI/Text/TextBase.h>

// The text of a canvas: its main mesh on its own CanvasRenderer (the font's first atlas page), every other mesh (other
// pages, fallback fonts, sprites, decorations) on a child sub mesh object of its own; masked and clipped as the canvas's
// graphics.
namespace helix::canvasui
{
    class CanvasText;

    // A sub mesh of a canvas text: a graphic its text sets the mesh, material and texture of.
    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class TextSubMesh : public MaskableGraphic
    {
    public:
        HX_PROPERTY(ReadOnly) CanvasText* textComponent() const;
        HX_PROPERTY() ui::FontAsset* fontAsset() const { return ui::ScriptObjectAs<ui::FontAsset>(font_.Get()); }
        void SetFontAsset(ui::FontAsset* value) { font_ = engine::PPtr<engine::Object>(value); }
        HX_PROPERTY(ReadOnly) engine::Texture* mainTexture() const override { return texture_.Get(); }
        HX_PROPERTY() engine::Material* sharedMaterial() const { return sharedMaterial_.Get(); }
        void SetSharedMaterial(engine::Material* value);
        HX_PROPERTY() engine::Material* fallbackMaterial() const { return nullptr; }
        void SetFallbackMaterial(engine::Material* value) { SetSharedMaterial(value); }
        HX_PROPERTY() engine::Material* fallbackSourceMaterial() const { return nullptr; }
        void SetFallbackSourceMaterial(engine::Material* value) { (void)value; }
        HX_PROPERTY() bool isDefaultMaterial() const { return false; }
        void SetIsDefaultMaterial(bool value) { (void)value; }
        HX_PROPERTY() f32 padding() const { return padding_; }
        void SetPadding(f32 value) { padding_ = value; }
        HX_PROPERTY(ReadOnly) engine::Mesh* mesh() const { return mesh_.Get(); }
        engine::Material* material() const override { return sharedMaterial_.Get() ? sharedMaterial_.Get() : MaskableGraphic::material(); }

        // What the text draws with it now.
        void Set(engine::Mesh* mesh, engine::Material* material, engine::Texture* texture);
        // A sub mesh object under the text (made, else the one at that index).
        static TextSubMesh* AddSubTextObject(CanvasText* text, i32 index);

        HX_FUNCTION(Protected) void UpdateGeometry() override;
        HX_FUNCTION(Protected) void UpdateMaterial() override;

    private:
        engine::PPtr<engine::Object> font_;
        engine::PPtr<engine::Mesh> mesh_;
        engine::PPtr<engine::Material> sharedMaterial_;
        engine::PPtr<engine::Texture> texture_;
        f32 padding_ = 0;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class CanvasText : public TextBase
    {
    public:
        HX_PROPERTY() Vector4 maskOffset() const { return maskOffset_; }
        void SetMaskOffset(Vector4 value) { maskOffset_ = value; }
        engine::Material* material() const override { return fontSharedMaterial(); }
        void SetMaterial(engine::Material* value) override { SetFontSharedMaterial(value); }
        engine::Material* defaultMaterial() const override { return fontSharedMaterial(); }
        HX_PROPERTY(ReadOnly) engine::Texture* mainTexture() const override;

        HX_FUNCTION() void Rebuild(CanvasUpdate update) override;
        HX_FUNCTION() void SetAllDirty() override;
        HX_FUNCTION() void SetMaterialDirty() override;
        HX_FUNCTION() void CalculateLayoutInputHorizontal() override {}
        HX_FUNCTION() void CalculateLayoutInputVertical() override {}
        HX_FUNCTION() void Cull(Rect clipRect, bool validRect) override;
        HX_FUNCTION() void RecalculateClipping() override;

        // The sub mesh objects of the text (index 1 is the first).
        Span<const engine::PPtr<engine::Object>> subMeshObjects() const { return Span<const engine::PPtr<engine::Object>>(subObjects_.Data(), subObjects_.Size()); }

        HX_FUNCTION(Protected) void UpdateGeometry() override;
        HX_FUNCTION(Protected) void UpdateMaterial() override;

    protected:
        void UploadMeshes() override;
        f32 PixelScale() const override;

    private:
        engine::Texture* TextureOf(usize mesh) const;

        HX_FIELD(SerializeField, Name = "m_maskOffset") Vector4 maskOffset_ = Vector4(0, 0, 0, 0);
        HX_FIELD(SerializeField, Name = "m_subTextObjects") Array<engine::PPtr<engine::Object>> subObjects_;
    };
}
