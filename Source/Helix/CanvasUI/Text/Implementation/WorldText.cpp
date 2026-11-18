#include <Helix/CanvasUI/Text/WorldText.h>

#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Engine/Transform/RectTransform.h>

#include <cmath>

namespace helix::canvasui
{
    namespace
    {
        void Draw(engine::Renderer* r, engine::MeshFilter* f, engine::MaterialPropertyBlock& block, engine::Mesh* mesh, engine::Material* material,
                  engine::Texture* texture)
        {
            if (f)
                f->SetSharedMesh(mesh);
            if (!r)
                return;
            r->SetSharedMaterial(material);
            block.Clear();
            if (texture)
                block.SetTexture("_MainTex", texture);
            r->SetPropertyBlock(&block);
        }
    }

    // ------------------------------------------------------------------------------------------------ text::TextSubMesh

    WorldText* text::TextSubMesh::textComponent() const
    {
        engine::Transform* p = engine::Component::transform()->parent();
        return p ? ComponentOf<WorldText>(p->gameObject()) : nullptr;
    }

    engine::Renderer* text::TextSubMesh::renderer() const { return gameObject()->GetComponent<engine::MeshRenderer>(); }
    engine::MeshFilter* text::TextSubMesh::meshFilter() const { return gameObject()->GetComponent<engine::MeshFilter>(); }

    void text::TextSubMesh::SetSharedMaterial(engine::Material* value)
    {
        material_ = engine::PPtr<engine::Material>(value);
        if (engine::Renderer* r = renderer())
            r->SetSharedMaterial(value);
    }

    void text::TextSubMesh::Set(engine::Mesh* mesh, engine::Material* material, engine::Texture* texture)
    {
        mesh_ = engine::PPtr<engine::Mesh>(mesh);
        material_ = engine::PPtr<engine::Material>(material);
        texture_ = engine::PPtr<engine::Texture>(texture);
        Draw(renderer(), meshFilter(), block_, mesh, material, texture);
    }

    text::TextSubMesh* text::TextSubMesh::AddSubTextObject(WorldText* text, i32 index)
    {
        if (!text || index < 1)
            return nullptr;
        Span<const engine::PPtr<engine::Object>> made = text->subMeshObjects();
        if (usize(index - 1) < made.Size())
            if (auto* s = static_cast<TextSubMesh*>(made[usize(index - 1)].Get()))
                return s;
        engine::GameObject* g = engine::GameObject::Create(Format("Text SubMesh [{}]", index));
        auto* rect = static_cast<engine::RectTransform*>(g->AddComponent(engine::ClassOf<engine::RectTransform>()));
        rect->SetParent(text->engine::Component::transform(), false);
        rect->SetAnchorMin(Vector2(0, 0));
        rect->SetAnchorMax(Vector2(1, 1));
        rect->SetSizeDelta(Vector2(0, 0));
        g->SetLayer(text->gameObject()->layer());
        return static_cast<TextSubMesh*>(g->AddComponent(scripting::TypeOf<TextSubMesh>()));
    }

    // ------------------------------------------------------------------------------------------------ WorldText

    engine::Renderer* WorldText::renderer() const { return gameObject()->GetComponent<engine::MeshRenderer>(); }
    engine::MeshFilter* WorldText::meshFilter() const { return gameObject()->GetComponent<engine::MeshFilter>(); }

    i32 WorldText::sortingLayerID() const { return renderer() ? renderer()->sortingLayerID() : 0; }

    void WorldText::SetSortingLayerID(i32 value)
    {
        if (engine::Renderer* r = renderer())
            r->SetSortingLayerID(value);
        for (const engine::PPtr<engine::Object>& o : subObjects_)
            if (auto* s = static_cast<text::TextSubMesh*>(o.Get()); s && s->renderer())
                s->renderer()->SetSortingLayerID(value);
    }

    i32 WorldText::sortingOrder() const { return renderer() ? renderer()->sortingOrder() : 0; }

    void WorldText::SetSortingOrder(i32 value)
    {
        if (engine::Renderer* r = renderer())
            r->SetSortingOrder(value);
        for (const engine::PPtr<engine::Object>& o : subObjects_)
            if (auto* s = static_cast<text::TextSubMesh*>(o.Get()); s && s->renderer())
                s->renderer()->SetSortingOrder(value);
    }

    void WorldText::SetMask(MaskingTypes type, Vector4 maskCoords) { SetMask(type, maskCoords, 0, 0); }

    void WorldText::SetMask(MaskingTypes type, Vector4 maskCoords, f32 softnessX, f32 softnessY)
    {
        maskType_ = type;
        maskCoords_ = maskCoords;
        maskSoftness_ = Vector2(softnessX, softnessY);
        SetMaterialDirty();
    }

    engine::Texture* WorldText::TextureOf(usize mesh) const
    {
        if (mesh < meshTextures_.Size() && meshTextures_[mesh].Get())
            return meshTextures_[mesh].Get();
        ui::FontAsset* f = font();
        return f ? f->AtlasTexture(0, false) : nullptr;
    }

    void WorldText::Rebuild(CanvasUpdate update)
    {
        if (update == CanvasUpdate::PreRender && changed_ && IsActive())
        {
            GenerateText();
            return;
        }
        MaskableGraphic::Rebuild(update);
    }

    void WorldText::SetAllDirty()
    {
        changed_ = true;
        MaskableGraphic::SetAllDirty();
    }

    void WorldText::UpdateGeometry()
    {
        if (changed_)
            GenerateText();
        else
            UploadMeshes();
    }

    void WorldText::UpdateMaterial() { Draw(renderer(), nullptr, block_, nullptr, fontSharedMaterial(), TextureOf(0)); }

    void WorldText::UploadMeshes()
    {
        Draw(renderer(), meshFilter(), block_, mesh_, fontSharedMaterial(), TextureOf(0));
        if (maskType_ != MaskingTypes::MaskOff)
        {
            block_.SetVector("_ClipRect", maskCoords_);
            block_.SetFloat("_MaskSoftnessX", maskSoftness_.x);
            block_.SetFloat("_MaskSoftnessY", maskSoftness_.y);
            if (engine::Renderer* r = renderer())
                r->SetPropertyBlock(&block_);
        }
        usize count = info_->meshInfo.Size();
        for (usize mi = 1; mi < count; mi++)
        {
            text::TextSubMesh* sub = text::TextSubMesh::AddSubTextObject(this, i32(mi));
            if (!sub)
                continue;
            if (mi - 1 >= subObjects_.Size())
                subObjects_.Push(engine::PPtr<engine::Object>(sub));
            const MeshInfo& m = info_->meshInfo[mi];
            sub->SetFontAsset(font());
            sub->Set(m.mesh, m.material, TextureOf(mi));
        }
        for (usize i = count > 0 ? count - 1 : 0; i < subObjects_.Size(); i++)
            if (auto* sub = static_cast<text::TextSubMesh*>(subObjects_[i].Get()))
                sub->Set(nullptr, nullptr, nullptr);
    }

    f32 WorldText::PixelScale() const
    {
        // Screen pixels per local unit from the main camera at the text's distance.
        f32 lossy = std::fabs(engine::Component::transform()->lossyScale().y);
        engine::Camera* camera = engine::Camera::main();
        if (!camera)
            return lossy > 0 ? lossy : 1;
        f32 height = f32(camera->pixelHeight());
        f32 perUnit = 1;
        if (camera->orthographic() || isOrthographic())
            perUnit = height / (2.0f * (camera->orthographic() ? camera->orthographicSize() : 5.0f));
        else
        {
            engine::Transform* ct = camera->transform();
            Vector3 forward = ct->rotation() * Vector3(0, 0, 1);
            f32 distance = std::fabs(Vector3::Dot(engine::Component::transform()->position() - ct->position(), forward));
            f32 half = std::tan(camera->fieldOfView() * 0.5f * 3.14159265f / 180.0f);
            perUnit = distance > 1e-4f && half > 1e-6f ? height / (2.0f * distance * half) : 1.0f;
        }
        f32 s = lossy * perUnit;
        return s > 0 ? s : 1;
    }
}
