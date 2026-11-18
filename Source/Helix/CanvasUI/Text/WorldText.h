#pragma once

#include <Helix/CanvasUI/Text/TextBase.h>
#include <Helix/Engine/Rendering/Renderer.h>

// The text of the scene: its main mesh drawn by its MeshRenderer (the font's first atlas page through a property
// block), every other mesh by a child sub mesh object with its own renderer; sorted as the scene's transparent objects.
namespace helix::canvasui
{
    class WorldText;

    namespace text
    {
        // A sub mesh of a world text: a renderer its text sets the mesh, material and texture of.
        HX_CLASS(Namespace = "HelixEngine.Text", RequireComponent = "engine::MeshRenderer, engine::MeshFilter")
        class TextSubMesh : public engine::GameBehaviour
        {
        public:
            HX_PROPERTY(ReadOnly) WorldText* textComponent() const;
            HX_PROPERTY() ui::FontAsset* fontAsset() const { return ui::ScriptObjectAs<ui::FontAsset>(font_.Get()); }
            void SetFontAsset(ui::FontAsset* value) { font_ = engine::PPtr<engine::Object>(value); }
            HX_PROPERTY(ReadOnly) engine::Texture* mainTexture() const { return texture_.Get(); }
            HX_PROPERTY() engine::Material* sharedMaterial() const { return material_.Get(); }
            void SetSharedMaterial(engine::Material* value);
            HX_PROPERTY() engine::Material* material() const { return material_.Get(); }
            void SetMaterial(engine::Material* value) { SetSharedMaterial(value); }
            HX_PROPERTY() engine::Material* fallbackMaterial() const { return nullptr; }
            void SetFallbackMaterial(engine::Material* value) { SetSharedMaterial(value); }
            HX_PROPERTY() engine::Material* fallbackSourceMaterial() const { return nullptr; }
            void SetFallbackSourceMaterial(engine::Material* value) { (void)value; }
            HX_PROPERTY() bool isDefaultMaterial() const { return false; }
            void SetIsDefaultMaterial(bool value) { (void)value; }
            HX_PROPERTY() f32 padding() const { return padding_; }
            void SetPadding(f32 value) { padding_ = value; }
            HX_PROPERTY(ReadOnly) engine::Mesh* mesh() const { return mesh_.Get(); }
            HX_PROPERTY(ReadOnly) engine::Renderer* renderer() const;
            HX_PROPERTY(ReadOnly) engine::MeshFilter* meshFilter() const;

            void Set(engine::Mesh* mesh, engine::Material* material, engine::Texture* texture);
            static TextSubMesh* AddSubTextObject(WorldText* text, i32 index);

        private:
            engine::PPtr<engine::Object> font_;
            engine::PPtr<engine::Mesh> mesh_;
            engine::PPtr<engine::Material> material_;
            engine::PPtr<engine::Texture> texture_;
            f32 padding_ = 0;
            engine::MaterialPropertyBlock block_;
        };
    }

    HX_CLASS(Namespace = "HelixEngine.Text", RequireComponent = "engine::MeshRenderer, engine::MeshFilter")
    class WorldText : public TextBase
    {
    public:
        f32 FontScale() const override { return isOrthographic() ? 1.0f : 0.1f; }
        HX_PROPERTY() i32 sortingLayerID() const;
        void SetSortingLayerID(i32 value);
        HX_PROPERTY() i32 sortingOrder() const;
        void SetSortingOrder(i32 value);
        HX_PROPERTY(ReadOnly) engine::Renderer* renderer() const;
        HX_PROPERTY(ReadOnly) engine::MeshFilter* meshFilter() const;
        HX_PROPERTY() MaskingTypes maskType() const { return maskType_; }
        void SetMaskType(MaskingTypes value) { maskType_ = value; }
        HX_FUNCTION() void SetMask(MaskingTypes type, Vector4 maskCoords);
        HX_FUNCTION() void SetMask(MaskingTypes type, Vector4 maskCoords, f32 softnessX, f32 softnessY);

        HX_FUNCTION() void Rebuild(CanvasUpdate update) override;
        HX_FUNCTION() void SetAllDirty() override;
        Span<const engine::PPtr<engine::Object>> subMeshObjects() const { return Span<const engine::PPtr<engine::Object>>(subObjects_.Data(), subObjects_.Size()); }

        HX_FUNCTION(Protected) void UpdateGeometry() override;
        HX_FUNCTION(Protected) void UpdateMaterial() override;

    protected:
        void UploadMeshes() override;
        f32 PixelScale() const override;

    private:
        engine::Texture* TextureOf(usize mesh) const;

        HX_FIELD(SerializeField, Name = "m_maskType") MaskingTypes maskType_ = MaskingTypes::MaskOff;
        Vector4 maskCoords_ = Vector4(0, 0, 0, 0);
        Vector2 maskSoftness_ = Vector2(0, 0);
        HX_FIELD(SerializeField, Name = "m_subTextObjects") Array<engine::PPtr<engine::Object>> subObjects_;
        engine::MaterialPropertyBlock block_;
    };
}
