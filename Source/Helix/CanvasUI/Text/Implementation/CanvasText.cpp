#include <Helix/CanvasUI/Text/CanvasText.h>

#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Canvas/CanvasRenderer.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Engine/Transform/RectTransform.h>

#include <cmath>

namespace helix::canvasui
{
    // ------------------------------------------------------------------------------------------------ TextSubMesh

    CanvasText* TextSubMesh::textComponent() const
    {
        engine::Transform* p = engine::Component::transform()->parent();
        return p ? ComponentOf<CanvasText>(p->gameObject()) : nullptr;
    }

    void TextSubMesh::SetSharedMaterial(engine::Material* value)
    {
        sharedMaterial_ = engine::PPtr<engine::Material>(value);
        SetMaterialDirty();
    }

    void TextSubMesh::Set(engine::Mesh* mesh, engine::Material* material, engine::Texture* texture)
    {
        mesh_ = engine::PPtr<engine::Mesh>(mesh);
        sharedMaterial_ = engine::PPtr<engine::Material>(material);
        texture_ = engine::PPtr<engine::Texture>(texture);
        if (engine::CanvasRenderer* r = canvasRenderer())
        {
            r->SetMesh(mesh);
            r->SetMaterial(materialForRendering(), texture);
        }
    }

    TextSubMesh* TextSubMesh::AddSubTextObject(CanvasText* text, i32 index)
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
        rect->SetPivot(text->rectTransform() ? text->rectTransform()->pivot() : Vector2(0.5f, 0.5f));
        g->SetLayer(text->gameObject()->layer());
        auto* sub = static_cast<TextSubMesh*>(g->AddComponent(scripting::TypeOf<TextSubMesh>()));
        if (sub)
            sub->SetRaycastTarget(false);
        return sub;
    }

    // The text sets the mesh: the graphic's own rebuild only uploads it again.
    void TextSubMesh::UpdateGeometry()
    {
        if (engine::CanvasRenderer* r = canvasRenderer())
            r->SetMesh(mesh_.Get());
    }

    void TextSubMesh::UpdateMaterial()
    {
        if (engine::CanvasRenderer* r = canvasRenderer())
            r->SetMaterial(materialForRendering(), texture_.Get());
    }

    // ------------------------------------------------------------------------------------------------ CanvasText

    engine::Texture* CanvasText::TextureOf(usize mesh) const
    {
        if (mesh < meshTextures_.Size() && meshTextures_[mesh].Get())
            return meshTextures_[mesh].Get();
        ui::FontAsset* f = font();
        return f ? f->AtlasTexture(0, false) : nullptr;
    }

    engine::Texture* CanvasText::mainTexture() const
    {
        engine::Texture* t = TextureOf(0);
        return t ? t : engine::Texture2D::whiteTexture();
    }

    void CanvasText::Rebuild(CanvasUpdate update)
    {
        // The text lays out in the rebuild's last stage before drawing (after the layout).
        if (update == CanvasUpdate::PreRender && changed_ && IsActive())
        {
            GenerateText();
            return;
        }
        MaskableGraphic::Rebuild(update);
    }

    void CanvasText::SetAllDirty()
    {
        changed_ = true;
        MaskableGraphic::SetAllDirty();
    }

    void CanvasText::SetMaterialDirty() { MaskableGraphic::SetMaterialDirty(); }

    void CanvasText::UpdateGeometry()
    {
        if (changed_)
            GenerateText();
        else
            UploadMeshes();
    }

    void CanvasText::UpdateMaterial()
    {
        if (engine::CanvasRenderer* r = canvasRenderer())
        {
            r->SetMaterialCount(1);
            r->SetMaterial(materialForRendering(), 0);
            r->SetTexture(mainTexture());
        }
        for (usize i = 0; i < subObjects_.Size(); i++)
            if (auto* s = static_cast<TextSubMesh*>(subObjects_[i].Get()))
                s->SetMaterialDirty();
    }

    void CanvasText::UploadMeshes()
    {
        engine::CanvasRenderer* r = canvasRenderer();
        if (r)
        {
            r->SetMesh(mesh_);
            UpdateMaterial();
        }
        // The other meshes on the sub mesh objects (made as needed, the rest emptied).
        usize count = info_->meshInfo.Size();
        for (usize mi = 1; mi < count; mi++)
        {
            TextSubMesh* sub = TextSubMesh::AddSubTextObject(this, i32(mi));
            if (!sub)
                continue;
            if (mi - 1 >= subObjects_.Size())
                subObjects_.Push(engine::PPtr<engine::Object>(sub));
            const MeshInfo& m = info_->meshInfo[mi];
            sub->SetFontAsset(font());
            sub->Set(m.mesh, m.material, TextureOf(mi));
        }
        for (usize i = count > 0 ? count - 1 : 0; i < subObjects_.Size(); i++)
            if (auto* sub = static_cast<TextSubMesh*>(subObjects_[i].Get()))
                sub->Set(nullptr, nullptr, nullptr);
    }

    f32 CanvasText::PixelScale() const
    {
        f32 lossy = std::fabs(engine::Component::transform()->lossyScale().y);
        engine::Canvas* c = const_cast<CanvasText*>(this)->canvas();
        engine::Canvas* root = c ? c->rootCanvas() : nullptr;
        if (!root || root->renderMode() == engine::RenderMode::ScreenSpaceOverlay)
            return lossy > 0 ? lossy : 1;
        engine::Camera* camera = root->worldCamera() ? root->worldCamera() : engine::Camera::main();
        if (!camera)
            return lossy > 0 ? lossy : 1;
        // Screen pixels per world unit at the text's distance from the camera.
        f32 height = f32(camera->pixelHeight());
        f32 perUnit = 1;
        if (camera->orthographic())
            perUnit = height / (2.0f * camera->orthographicSize());
        else
        {
            engine::Transform* ct = camera->transform();
            Vector3 forward = ct->rotation() * Vector3(0, 0, 1);
            Vector3 d = engine::Component::transform()->position() - ct->position();
            f32 distance = std::fabs(Vector3::Dot(d, forward));
            f32 half = std::tan(camera->fieldOfView() * 0.5f * 3.14159265f / 180.0f);
            perUnit = distance > 1e-4f && half > 1e-6f ? height / (2.0f * distance * half) : 1.0f;
        }
        f32 s = lossy * perUnit;
        return s > 0 ? s : 1;
    }

    void CanvasText::Cull(Rect clipRect, bool validRect)
    {
        MaskableGraphic::Cull(clipRect, validRect);
        for (const engine::PPtr<engine::Object>& o : subObjects_)
            if (auto* s = static_cast<TextSubMesh*>(o.Get()))
                s->Cull(clipRect, validRect);
    }

    void CanvasText::RecalculateClipping()
    {
        MaskableGraphic::RecalculateClipping();
        for (const engine::PPtr<engine::Object>& o : subObjects_)
            if (auto* s = static_cast<TextSubMesh*>(o.Get()))
                s->RecalculateClipping();
    }
}
