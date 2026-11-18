#include <Helix/CanvasUI/Masking/Masks.h>

#include <Helix/CanvasUI/Core/Implementation/CanvasUILog.h>
#include <Helix/CanvasUI/Core/ManagedCalls.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Canvas/CanvasRenderer.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Transform/RectTransform.h>
#include <Helix/Engine/Transform/RectTransformUtility.h>

#include <cmath>

namespace helix::canvasui
{
    namespace
    {
        // The axis aligned rectangle of a RectTransform's corners in a transform's space.
        Rect RectIn(engine::RectTransform* rt, engine::Transform* space)
        {
            Vector3 corners[4];
            rt->GetWorldCorners(corners);
            Matrix4x4 toSpace = space ? space->worldToLocalMatrix() : Matrix4x4::identity;
            Vector2 lo(0, 0), hi(0, 0);
            for (int i = 0; i < 4; i++)
            {
                Vector3 p = toSpace.MultiplyPoint(corners[i]);
                if (i == 0)
                    lo = hi = Vector2(p.x, p.y);
                lo = Vector2(p.x < lo.x ? p.x : lo.x, p.y < lo.y ? p.y : lo.y);
                hi = Vector2(p.x > hi.x ? p.x : hi.x, p.y > hi.y ? p.y : hi.y);
            }
            return Rect(lo.x, lo.y, hi.x - lo.x, hi.y - lo.y);
        }

        // Overlap of two rectangles (edges touching do not count), either one possibly of negative size.
        bool Overlaps(Rect a, Rect b)
        {
            f32 ax0 = a.xMin() < a.xMax() ? a.xMin() : a.xMax(), ax1 = a.xMin() < a.xMax() ? a.xMax() : a.xMin();
            f32 ay0 = a.yMin() < a.yMax() ? a.yMin() : a.yMax(), ay1 = a.yMin() < a.yMax() ? a.yMax() : a.yMin();
            f32 bx0 = b.xMin() < b.xMax() ? b.xMin() : b.xMax(), bx1 = b.xMin() < b.xMax() ? b.xMax() : b.xMin();
            f32 by0 = b.yMin() < b.yMax() ? b.yMin() : b.yMax(), by1 = b.yMin() < b.yMax() ? b.yMax() : b.yMin();
            return bx1 > ax0 && bx0 < ax1 && by1 > ay0 && by0 < ay1;
        }

        // The canvases from the transform up (itself first).
        void CanvasesAbove(engine::Transform* t, Array<engine::Canvas*>& out)
        {
            out.Clear();
            for (; t; t = t->parent())
                if (engine::GameObject* g = t->gameObject(); g && g->activeInHierarchy())
                    if (engine::Canvas* c = ComponentOf<engine::Canvas>(g))
                        out.Push(c);
        }

        template <class T>
        void ComponentsAbove(engine::Transform* t, Array<T*>& out)
        {
            out.Clear();
            for (; t; t = t->parent())
                if (engine::GameObject* g = t->gameObject(); g && g->activeInHierarchy())
                    if (T* c = ComponentOf<T>(g))
                        out.Push(c);
        }

        // The components below the GameObject (itself too), depth first, active ones.
        void ComponentsBelow(engine::Transform* t, Array<engine::Component*>& out)
        {
            engine::GameObject* g = t->gameObject();
            if (!g || !g->activeInHierarchy())
                return;
            for (u32 i = 0; i < g->GetComponentCount(); i++)
                if (engine::Component* c = g->GetComponentAtIndex(i))
                    out.Push(c);
            for (u32 i = 0; i < t->childCount(); i++)
                ComponentsBelow(t->GetChild(i), out);
        }
    }

    // ------------------------------------------------------------------------------------------- MaskableGraphic

    MaskableGraphic::MaskableGraphic()
    {
        static bool registered = false;
        if (registered)
            return;
        registered = true;
        engine::NativeEventEntry e;
        e.storage = [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<MaskableGraphic*>(o)->onCullStateChanged(); };
        e.raise = [](engine::Object* o, Span<const scripting::Variant> a) {
            bool v = false;
            if (!a.Empty())
                a[0].Get(v);
            static_cast<MaskableGraphic*>(o)->onCullStateChanged().Invoke(v);
        };
        engine::RegisterNativeEvent(scripting::TypeOf<MaskableGraphic>(), 0, e);
    }

    MaskableGraphic::~MaskableGraphic() = default;

    void MaskableGraphic::SetMaskable(bool value)
    {
        if (value == maskable_)
            return;
        maskable_ = value;
        shouldRecalculateStencil_ = true;
        SetMaterialDirty();
    }

    void MaskableGraphic::SetIsMaskingGraphic(bool value) { isMaskingGraphic_ = value; }

    engine::Material* MaskableGraphic::GetModifiedMaterial(engine::Material* baseMaterial)
    {
        engine::Material* toUse = baseMaterial;
        if (shouldRecalculateStencil_)
        {
            if (maskable_)
            {
                engine::Transform* root = MaskUtilities::FindRootSortOverrideCanvas(engine::Component::transform());
                stencilValue_ = MaskUtilities::GetStencilDepth(engine::Component::transform(), root);
            }
            else
                stencilValue_ = 0;
            shouldRecalculateStencil_ = false;
        }
        if (stencilValue_ > 0 && !isMaskingGraphic_)
        {
            i32 bits = (1 << stencilValue_) - 1;
            engine::Material* maskMat =
                StencilMaterial::Add(toUse, bits, engine::StencilOp::Keep, engine::CompareFunction::Equal, engine::ColorWriteMask::All, bits, 0);
            StencilMaterial::Remove(maskMaterial_.Get());
            maskMaterial_ = engine::PPtr<engine::Material>(maskMat);
            toUse = maskMat;
        }
        return toUse;
    }

    void MaskableGraphic::Cull(Rect clipRect, bool validRect)
    {
        engine::RectTransform* rt = rectTransform();
        engine::Canvas* c = canvas();
        bool cull = !validRect;
        if (!cull && rt)
            cull = !Overlaps(clipRect, RectIn(rt, c ? c->rootCanvas()->transform() : nullptr));
        UpdateCull(cull);
    }

    void MaskableGraphic::UpdateCull(bool cull)
    {
        engine::CanvasRenderer* r = canvasRenderer();
        if (!r || r->cull() == cull)
            return;
        r->SetCull(cull);
        engine::UISystemProfilerApi::AddMarker("MaskableGraphic.cullingChanged", this);
        onCullStateChanged_.Invoke(cull);
        OnCullingChanged();
    }

    void MaskableGraphic::SetClipRect(Rect clipRect, bool validRect)
    {
        if (engine::CanvasRenderer* r = canvasRenderer())
        {
            if (validRect)
                r->EnableRectClipping(clipRect);
            else
                r->DisableRectClipping();
        }
    }

    void MaskableGraphic::SetClipSoftness(Vector2 clipSoftness)
    {
        if (engine::CanvasRenderer* r = canvasRenderer())
            r->SetClippingSoftness(clipSoftness);
    }

    void MaskableGraphic::RecalculateClipping() { UpdateClipParent(); }

    void MaskableGraphic::RecalculateMasking()
    {
        StencilMaterial::Remove(maskMaterial_.Get());
        maskMaterial_ = engine::PPtr<engine::Material>();
        shouldRecalculateStencil_ = true;
        SetMaterialDirty();
    }

    bool MaskableGraphic::Raycast(Vector2 sp, engine::Camera* eventCamera) { return RaycastIgnoringMasks(sp, eventCamera, !maskable_); }

    void MaskableGraphic::UpdateClipParent()
    {
        RectMask2D* next = maskable_ && IsActive() ? MaskUtilities::GetRectMaskForClippable(this) : nullptr;
        auto* current = static_cast<RectMask2D*>(parentMask_.Get());
        if (current && (next != current || !next->IsActive()))
        {
            current->RemoveClippable(this);
            UpdateCull(false);
        }
        if (next && next->IsActive())
            next->AddClippable(this);
        parentMask_ = engine::PPtr<engine::Object>(next);
    }

    void MaskableGraphic::OnEnable()
    {
        Graphic::OnEnable();
        if (isMaskingGraphic_)
            MaskUtilities::NotifyStencilStateChanged(this);
        shouldRecalculateStencil_ = true;
        UpdateClipParent();
        SetMaterialDirty();
    }

    void MaskableGraphic::OnDisable()
    {
        Graphic::OnDisable();
        shouldRecalculateStencil_ = true;
        SetMaterialDirty();
        UpdateClipParent();
        StencilMaterial::Remove(maskMaterial_.Get());
        maskMaterial_ = engine::PPtr<engine::Material>();
        if (isMaskingGraphic_)
            MaskUtilities::NotifyStencilStateChanged(this);
    }

    void MaskableGraphic::OnTransformParentChanged()
    {
        Graphic::OnTransformParentChanged();
        if (!isActiveAndEnabled())
            return;
        shouldRecalculateStencil_ = true;
        UpdateClipParent();
        SetMaterialDirty();
    }

    void MaskableGraphic::OnCanvasHierarchyChanged()
    {
        Graphic::OnCanvasHierarchyChanged();
        if (!isActiveAndEnabled())
            return;
        shouldRecalculateStencil_ = true;
        UpdateClipParent();
        SetMaterialDirty();
    }

    // ------------------------------------------------------------------------------------------------------ Mask

    Mask::~Mask()
    {
        StencilMaterial::Remove(maskMaterial_.Get());
        StencilMaterial::Remove(unmaskMaterial_.Get());
    }


    engine::RectTransform* Mask::rectTransform() const
    {
        engine::Transform* t = engine::Component::transform();
        return t && t->Is<engine::RectTransform>() ? static_cast<engine::RectTransform*>(t) : nullptr;
    }

    Graphic* Mask::graphic() const { return ComponentOf<Graphic>(gameObject()); }

    void Mask::SetShowMaskGraphic(bool value)
    {
        if (showMaskGraphic_ == value)
            return;
        showMaskGraphic_ = value;
        if (Graphic* g = graphic())
            g->SetMaterialDirty();
    }

    bool Mask::IsRaycastLocationValid(Vector2 sp, engine::Camera* eventCamera)
    {
        if (!isActiveAndEnabled())
            return true;
        return engine::RectTransformUtility::RectangleContainsScreenPoint(rectTransform(), sp, eventCamera);
    }

    engine::Material* Mask::GetModifiedMaterial(engine::Material* baseMaterial)
    {
        if (!MaskEnabled())
            return baseMaterial;
        engine::Transform* root = MaskUtilities::FindRootSortOverrideCanvas(engine::Component::transform());
        i32 depth = MaskUtilities::GetStencilDepth(engine::Component::transform(), root);
        if (depth >= 8)
        {
            HX_LOG_WARNING(CanvasUILog, "Attempting to use a stencil mask with depth > 8 ({}).", gameObject()->Name());
            return baseMaterial;
        }
        i32 bit = 1 << depth;
        engine::ColorWriteMask colors = showMaskGraphic_ ? engine::ColorWriteMask::All : engine::ColorWriteMask(0);
        engine::Material *mask = nullptr, *unmask = nullptr;
        if (bit == 1)
        {
            mask = StencilMaterial::Add(baseMaterial, 1, engine::StencilOp::Replace, engine::CompareFunction::Always, colors);
            unmask = StencilMaterial::Add(baseMaterial, 1, engine::StencilOp::Zero, engine::CompareFunction::Always, engine::ColorWriteMask(0));
        }
        else
        {
            mask = StencilMaterial::Add(baseMaterial, bit | (bit - 1), engine::StencilOp::Replace, engine::CompareFunction::Equal, colors, bit - 1, bit | (bit - 1));
            unmask = StencilMaterial::Add(baseMaterial, bit - 1, engine::StencilOp::Replace, engine::CompareFunction::Equal, engine::ColorWriteMask(0), bit - 1,
                                          bit | (bit - 1));
        }
        StencilMaterial::Remove(maskMaterial_.Get());
        maskMaterial_ = engine::PPtr<engine::Material>(mask);
        StencilMaterial::Remove(unmaskMaterial_.Get());
        unmaskMaterial_ = engine::PPtr<engine::Material>(unmask);
        if (engine::CanvasRenderer* r = graphic()->canvasRenderer())
        {
            r->SetPopMaterialCount(1);
            r->SetPopMaterial(unmask, 0);
        }
        return mask;
    }

    void Mask::OnEnable()
    {
        UIBehaviour::OnEnable();
        if (Graphic* g = graphic())
        {
            if (engine::CanvasRenderer* r = g->canvasRenderer())
                r->SetHasPopInstruction(true);
            g->SetMaterialDirty();
            if (scripting::IsAssignableFrom(scripting::TypeOf<MaskableGraphic>(), g->script() ? g->script()->type : scripting::TypeId()))
                static_cast<MaskableGraphic*>(g)->SetIsMaskingGraphic(true);
        }
        MaskUtilities::NotifyStencilStateChanged(this);
    }

    void Mask::OnDisable()
    {
        UIBehaviour::OnDisable();
        if (Graphic* g = graphic())
        {
            g->SetMaterialDirty();
            if (engine::CanvasRenderer* r = g->canvasRenderer())
            {
                r->SetHasPopInstruction(false);
                r->SetPopMaterialCount(0);
            }
            if (scripting::IsAssignableFrom(scripting::TypeOf<MaskableGraphic>(), g->script() ? g->script()->type : scripting::TypeId()))
                static_cast<MaskableGraphic*>(g)->SetIsMaskingGraphic(false);
        }
        StencilMaterial::Remove(maskMaterial_.Get());
        maskMaterial_ = engine::PPtr<engine::Material>();
        StencilMaterial::Remove(unmaskMaterial_.Get());
        unmaskMaterial_ = engine::PPtr<engine::Material>();
        MaskUtilities::NotifyStencilStateChanged(this);
    }

    // ------------------------------------------------------------------------------------------------ RectMask2D

    RectMask2D::~RectMask2D() { ClipperRegistry::Unregister(this); }


    engine::RectTransform* RectMask2D::rectTransform() const
    {
        engine::Transform* t = engine::Component::transform();
        return t && t->Is<engine::RectTransform>() ? static_cast<engine::RectTransform*>(t) : nullptr;
    }

    void RectMask2D::SetPadding(Vector4 value)
    {
        padding_ = value;
        MaskUtilities::Notify2DMaskStateChanged(this);
    }

    void RectMask2D::SetSoftness(Vector2Int value)
    {
        softness_ = Vector2Int(value.x > 0 ? value.x : 0, value.y > 0 ? value.y : 0);
        MaskUtilities::Notify2DMaskStateChanged(this);
    }

    engine::Canvas* RectMask2D::RootCanvas()
    {
        if (!canvas_.Get())
        {
            Array<engine::Canvas*> list;
            CanvasesAbove(engine::Component::transform(), list);
            canvas_ = engine::PPtr<engine::Canvas>(list.Empty() ? nullptr : list[list.Size() - 1]);
        }
        return canvas_.Get();
    }

    Rect RectMask2D::canvasRect()
    {
        engine::Canvas* c = RootCanvas();
        engine::RectTransform* rt = rectTransform();
        return c && rt ? RectIn(rt, c->transform()) : Rect(0, 0, 0, 0);
    }

    Rect RectMask2D::PaddedCanvasRect()
    {
        Rect r = canvasRect();
        return Rect(r.x + padding_.x, r.y + padding_.y, r.width - padding_.x - padding_.z, r.height - padding_.y - padding_.w);
    }

    bool RectMask2D::IsRaycastLocationValid(Vector2 sp, engine::Camera* eventCamera)
    {
        if (!isActiveAndEnabled())
            return true;
        return engine::RectTransformUtility::RectangleContainsScreenPoint(rectTransform(), sp, eventCamera, padding_);
    }

    void RectMask2D::PerformClipping()
    {
        engine::Canvas* canvas = RootCanvas();
        if (!canvas)
            return;
        if (shouldRecalculateClipRects_)
        {
            clippers_ = MaskUtilities::GetRectMasksForClipInternal(this);
            shouldRecalculateClipRects_ = false;
        }
        bool valid = true;
        Rect clip = Clipping::FindCullAndClipWorldRect(Span<RectMask2D* const>(clippers_.Data(), clippers_.Size()), valid);
        engine::Canvas* root = canvas->rootCanvas();
        engine::RenderMode mode = root->renderMode();
        if (mode == engine::RenderMode::ScreenSpaceCamera || mode == engine::RenderMode::ScreenSpaceOverlay)
        {
            engine::Transform* rt = root->transform();
            Rect rootRect = rt->Is<engine::RectTransform>() ? static_cast<engine::RectTransform*>(rt)->rect() : Rect(0, 0, 0, 0);
            if (!Overlaps(clip, rootRect))
                clip = Rect(0, 0, 0, 0), valid = false;
        }
        bool changed = clip.x != lastClipRect_.x || clip.y != lastClipRect_.y || clip.width != lastClipRect_.width || clip.height != lastClipRect_.height;
        for (usize i = 0; i < clippables_.Size();)
        {
            if (clippables_[i].object.Get() == nullptr)
            {
                clippables_.RemoveAt(i);
                continue;
            }
            IClippable* c = clippables_[i].clippable;
            auto* graphic = AsUIBehaviour(static_cast<engine::Component*>(clippables_[i].object.Get()));
            bool maskable = graphic && scripting::IsAssignableFrom(scripting::TypeOf<MaskableGraphic>(), graphic->script()->type);
            if (changed || forceClip_)
            {
                c->SetClipRect(clip, valid);
                if (maskable)
                    c->Cull(clip, valid);
            }
            else if (maskable)
                if (engine::CanvasRenderer* r = static_cast<MaskableGraphic*>(graphic)->canvasRenderer(); r && r->hasMoved())
                    c->Cull(clip, valid);
            i++;
        }
        lastClipRect_ = clip;
        lastValid_ = valid;
        forceClip_ = false;
        UpdateClipSoftness();
    }

    void RectMask2D::UpdateClipSoftness()
    {
        if (!RootCanvas())
            return;
        for (const Clippable& c : clippables_)
            if (c.object.Get())
                c.clippable->SetClipSoftness(Vector2(f32(softness_.x), f32(softness_.y)));
    }

    void RectMask2D::AddClippable(IClippable* clippable)
    {
        if (!clippable)
            return;
        shouldRecalculateClipRects_ = true;
        for (const Clippable& c : clippables_)
            if (c.clippable == clippable)
                return;
        Clippable c;
        c.clippable = clippable;
        // The component that is the clippable: the entry goes with it.
        Array<UIBehaviour*> behaviours;
        UIBehavioursOf(clippable->ClippableGameObject(), behaviours);
        for (UIBehaviour* b : behaviours)
            if (b->AsClippable() == clippable)
                c.object = engine::PPtr<engine::Object>(b);
        if (!c.object.Get())
            return;
        clippables_.Push(c);
        forceClip_ = true;
    }

    void RectMask2D::RemoveClippable(IClippable* clippable)
    {
        if (!clippable)
            return;
        shouldRecalculateClipRects_ = true;
        clippable->SetClipRect(Rect(0, 0, 0, 0), false);
        for (usize i = 0; i < clippables_.Size(); i++)
            if (clippables_[i].clippable == clippable)
            {
                clippables_.RemoveAt(i);
                break;
            }
        forceClip_ = true;
    }

    void RectMask2D::OnEnable()
    {
        UIBehaviour::OnEnable();
        shouldRecalculateClipRects_ = true;
        ClipperRegistry::Register(this);
        MaskUtilities::Notify2DMaskStateChanged(this);
    }

    void RectMask2D::OnDisable()
    {
        UIBehaviour::OnDisable();
        clippables_.Clear();
        clippers_.Clear();
        ClipperRegistry::Disable(this);
        MaskUtilities::Notify2DMaskStateChanged(this);
    }

    void RectMask2D::OnDestroy()
    {
        ClipperRegistry::Unregister(this);
        UIBehaviour::OnDestroy();
    }

    void RectMask2D::OnTransformParentChanged()
    {
        canvas_ = engine::PPtr<engine::Canvas>();
        UIBehaviour::OnTransformParentChanged();
        shouldRecalculateClipRects_ = true;
    }

    void RectMask2D::OnCanvasHierarchyChanged()
    {
        canvas_ = engine::PPtr<engine::Canvas>();
        UIBehaviour::OnCanvasHierarchyChanged();
        shouldRecalculateClipRects_ = true;
    }

    // ------------------------------------------------------------------------------------------- ClipperRegistry

    namespace
    {
        struct ClipperEntry
        {
            IClipper* clipper = nullptr;
            engine::PPtr<engine::Object> object;
            bool hasObject = false, disabled = false;
        };

        Array<ClipperEntry>& Clippers()
        {
            static Array<ClipperEntry>* c = new Array<ClipperEntry>();
            return *c;
        }

        i32 FindClipper(IClipper* c)
        {
            Array<ClipperEntry>& list = Clippers();
            for (usize i = 0; i < list.Size(); i++)
                if (list[i].clipper == c)
                    return i32(i);
            return -1;
        }
    }

    void ClipperRegistry::Register(IClipper* c)
    {
        if (!c)
            return;
        if (i32 i = FindClipper(c); i >= 0)
        {
            Clippers()[u32(i)].disabled = false;
            return;
        }
        ClipperEntry e;
        e.clipper = c;
        if (engine::Object* o = c->ClipperObject())
            e.object = engine::PPtr<engine::Object>(o), e.hasObject = true;
        Clippers().Push(e);
    }

    void ClipperRegistry::Unregister(IClipper* c)
    {
        if (i32 i = FindClipper(c); i >= 0)
            Clippers().RemoveAt(u32(i));
    }

    void ClipperRegistry::Disable(IClipper* c)
    {
        if (i32 i = FindClipper(c); i >= 0)
            Clippers()[u32(i)].disabled = true;
    }

    void ClipperRegistry::Cull()
    {
        Array<ClipperEntry>& list = Clippers();
        for (usize i = 0; i < list.Size();)
        {
            if (list[i].hasObject && !list[i].object.Get())
            {
                list.RemoveAt(i);
                continue;
            }
            if (!list[i].disabled)
                list[i].clipper->PerformClipping();
            i++;
        }
    }

    // -------------------------------------------------------------------------------------------------- Clipping

    Rect Clipping::FindCullAndClipWorldRect(Span<RectMask2D* const> rectMaskParents, bool& validRect)
    {
        validRect = false;
        if (rectMaskParents.Empty())
            return Rect(0, 0, 0, 0);
        f32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool first = true;
        for (RectMask2D* m : rectMaskParents)
        {
            if (!m)
                continue;
            Rect r = m->PaddedCanvasRect();
            if (first)
                x0 = r.xMin(), y0 = r.yMin(), x1 = r.xMax(), y1 = r.yMax(), first = false;
            else
            {
                x0 = r.xMin() > x0 ? r.xMin() : x0;
                y0 = r.yMin() > y0 ? r.yMin() : y0;
                x1 = r.xMax() < x1 ? r.xMax() : x1;
                y1 = r.yMax() < y1 ? r.yMax() : y1;
            }
        }
        if (first || x1 <= x0 || y1 <= y0)
            return Rect(0, 0, 0, 0);
        validRect = true;
        return Rect(x0, y0, x1 - x0, y1 - y0);
    }

    // ---------------------------------------------------------------------------------------------- MaskUtilities

    void MaskUtilities::Notify2DMaskStateChanged(engine::Component* mask)
    {
        if (!mask || !mask->transform())
            return;
        Array<engine::Component*> all;
        ComponentsBelow(mask->transform(), all);
        for (engine::Component* c : all)
        {
            if (c->gameObject() == mask->gameObject())
                continue;
            if (UIBehaviour* b = AsUIBehaviour(c))
                if (IClippable* clippable = b->AsClippable())
                    clippable->RecalculateClipping();
        }
    }

    void MaskUtilities::NotifyStencilStateChanged(engine::Component* mask)
    {
        if (!mask || !mask->transform())
            return;
        Array<engine::Component*> all;
        ComponentsBelow(mask->transform(), all);
        for (engine::Component* c : all)
        {
            if (c->gameObject() == mask->gameObject())
                continue;
            UIBehaviour* b = AsUIBehaviour(c);
            if (b && scripting::IsAssignableFrom(scripting::TypeOf<MaskableGraphic>(), b->script()->type))
                static_cast<MaskableGraphic*>(b)->RecalculateMasking();
        }
    }

    engine::Transform* MaskUtilities::FindRootSortOverrideCanvas(engine::Transform* start)
    {
        Array<engine::Canvas*> list;
        CanvasesAbove(start, list);
        engine::Canvas* canvas = nullptr;
        for (engine::Canvas* c : list)
        {
            canvas = c;
            if (c->overrideSorting())
                break;
        }
        return canvas ? canvas->transform() : nullptr;
    }

    i32 MaskUtilities::GetStencilDepth(engine::Transform* transform, engine::Transform* stopAfter)
    {
        i32 depth = 0;
        if (!transform || transform == stopAfter)
            return depth;
        for (engine::Transform* t = transform->parent(); t; t = t->parent())
        {
            if (Mask* m = ComponentOf<Mask>(t->gameObject()); m && m->MaskEnabled() && m->graphic()->IsActive())
                depth++;
            if (t == stopAfter)
                break;
        }
        return depth;
    }

    bool MaskUtilities::IsDescendantOrSelf(engine::Transform* father, engine::Transform* child)
    {
        if (!father || !child)
            return false;
        if (father == child)
            return true;
        for (engine::Transform* p = child->parent(); p; p = p->parent())
            if (p == father)
                return true;
        return false;
    }

    RectMask2D* MaskUtilities::GetRectMaskForClippable(IClippable* clippable)
    {
        if (!clippable || !clippable->ClippableGameObject())
            return nullptr;
        engine::Transform* start = clippable->ClippableGameObject()->transform();
        Array<RectMask2D*> masks;
        ComponentsAbove(start, masks);
        Array<engine::Canvas*> canvases;
        CanvasesAbove(start, canvases);
        for (RectMask2D* m : masks)
        {
            if (m->gameObject() == clippable->ClippableGameObject() || !m->isActiveAndEnabled())
                continue;
            for (usize i = canvases.Size(); i-- > 0;)
                if (!IsDescendantOrSelf(canvases[i]->transform(), m->transform()) && canvases[i]->overrideSorting())
                    return nullptr;
            return m;
        }
        return nullptr;
    }

    Array<RectMask2D*> MaskUtilities::GetRectMasksForClipInternal(RectMask2D* clipper)
    {
        Array<RectMask2D*> out;
        if (!clipper)
            return out;
        Array<RectMask2D*> masks;
        ComponentsAbove(clipper->transform(), masks);
        Array<engine::Canvas*> canvases;
        CanvasesAbove(clipper->transform(), canvases);
        for (RectMask2D* m : masks)
        {
            if (!m->IsActive())
                continue;
            bool add = true;
            for (usize j = canvases.Size(); j-- > 0 && add;)
                if (!IsDescendantOrSelf(canvases[j]->transform(), m->transform()) && canvases[j]->overrideSorting())
                    add = false;
            if (add)
                out.Push(m);
        }
        return out;
    }

    // -------------------------------------------------------------------------------------------- StencilMaterial

    namespace
    {
        struct StencilEntry
        {
            engine::PPtr<engine::Material> base, custom;
            i32 stencilId = 0, readMask = 0, writeMask = 0, count = 0;
            engine::StencilOp operation = engine::StencilOp::Keep;
            engine::CompareFunction compare = engine::CompareFunction::Always;
            engine::ColorWriteMask colorMask = engine::ColorWriteMask::All;
            bool alphaClip = false;
        };

        Array<StencilEntry>& Stencils()
        {
            static Array<StencilEntry>* s = new Array<StencilEntry>();
            return *s;
        }
    }

    engine::Material* StencilMaterial::Add(engine::Material* baseMat, i32 stencilID, engine::StencilOp operation, engine::CompareFunction compareFunction,
                                           engine::ColorWriteMask colorWriteMask)
    {
        return Add(baseMat, stencilID, operation, compareFunction, colorWriteMask, 255, 255);
    }

    engine::Material* StencilMaterial::Add(engine::Material* baseMat, i32 stencilID, engine::StencilOp operation, engine::CompareFunction compareFunction,
                                           engine::ColorWriteMask colorWriteMask, i32 readMask, i32 writeMask)
    {
        if ((stencilID <= 0 && colorWriteMask == engine::ColorWriteMask::All) || !baseMat)
            return baseMat;
        for (const char* p : {"_Stencil", "_StencilOp", "_StencilComp", "_StencilReadMask", "_StencilWriteMask", "_ColorMask"})
            if (!baseMat->HasProperty(StringView(p)))
            {
                HX_LOG_WARNING(CanvasUILog, "Material {} doesn't have {} property", baseMat->Name(), p);
                return baseMat;
            }
        Array<StencilEntry>& list = Stencils();
        for (usize i = 0; i < list.Size();)
        {
            if (!list[i].custom.Get())
            {
                list.RemoveAt(i);
                continue;
            }
            StencilEntry& e = list[i];
            if (e.base.Get() == baseMat && e.stencilId == stencilID && e.operation == operation && e.compare == compareFunction && e.readMask == readMask &&
                e.writeMask == writeMask && e.colorMask == colorWriteMask)
            {
                e.count++;
                return e.custom.Get();
            }
            i++;
        }
        StencilEntry e;
        e.count = 1;
        e.base = engine::PPtr<engine::Material>(baseMat);
        e.stencilId = stencilID, e.operation = operation, e.compare = compareFunction, e.readMask = readMask, e.writeMask = writeMask;
        e.colorMask = colorWriteMask;
        e.alphaClip = operation != engine::StencilOp::Keep && writeMask > 0;
        engine::Material* m = engine::Material::Create(*baseMat);
        m->hideFlags = engine::HideFlags::HideAndDontSave;
        m->SetName(Format("Stencil Id:{}, Op:{}, Comp:{}, WriteMask:{}, ReadMask:{}, ColorMask:{} AlphaClip:{} ({})", stencilID, i32(operation),
                          i32(compareFunction), writeMask, readMask, i32(colorWriteMask), e.alphaClip, baseMat->Name()));
        m->SetFloat("_Stencil", f32(stencilID));
        m->SetFloat("_StencilOp", f32(i32(operation)));
        m->SetFloat("_StencilComp", f32(i32(compareFunction)));
        m->SetFloat("_StencilReadMask", f32(readMask));
        m->SetFloat("_StencilWriteMask", f32(writeMask));
        m->SetFloat("_ColorMask", f32(i32(colorWriteMask)));
        m->SetFloat("_UseUIAlphaClip", e.alphaClip ? 1.0f : 0.0f);
        if (e.alphaClip)
            m->EnableKeyword("HELIX_UI_ALPHACLIP");
        else
            m->DisableKeyword("HELIX_UI_ALPHACLIP");
        e.custom = engine::PPtr<engine::Material>(m);
        list.Push(e);
        return m;
    }

    void StencilMaterial::Remove(engine::Material* customMat)
    {
        if (!customMat)
            return;
        Array<StencilEntry>& list = Stencils();
        for (usize i = 0; i < list.Size(); i++)
            if (list[i].custom.Get() == customMat)
            {
                if (--list[i].count == 0)
                {
                    engine::Object::DestroyImmediate(customMat);
                    list.RemoveAt(i);
                }
                return;
            }
    }

    void StencilMaterial::ClearAll()
    {
        Array<StencilEntry>& list = Stencils();
        for (StencilEntry& e : list)
            if (engine::Material* m = e.custom.Get())
                engine::Object::DestroyImmediate(m);
        list.Clear();
    }

    u32 StencilMaterial::Count() { return u32(Stencils().Size()); }

    void Mask::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        maskMaterial_ = engine::PPtr<engine::Material>();
        unmaskMaterial_ = engine::PPtr<engine::Material>();
    }

    void RectMask2D::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        clippables_.Clear();
        clippers_.Clear();
        canvas_ = engine::PPtr<engine::Canvas>();
        lastValid_ = false;
        forceClip_ = true;
        shouldRecalculateClipRects_ = true;
    }
}
