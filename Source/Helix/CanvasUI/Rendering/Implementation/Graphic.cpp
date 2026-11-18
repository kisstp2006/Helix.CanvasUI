#include <Helix/CanvasUI/Rendering/Graphic.h>

#include <Helix/CanvasUI/Core/ManagedCalls.h>
#include <Helix/CanvasUI/Layout/LayoutRebuilder.h>
#include <Helix/CanvasUI/Masking/Masks.h>
#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Canvas/CanvasRenderer.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Engine/Transform/RectTransform.h>
#include <Helix/Engine/Transform/RectTransformUtility.h>

namespace helix::canvasui
{
    namespace
    {
        struct Graphics
        {
            engine::PPtr<engine::Mesh> worker;
            VertexHelper* helper = nullptr;
            // The graphics with a color tween running.
            Array<engine::PPtr<engine::Object>> tweening;
            bool tweensHooked = false;
            // C#'s dirty callbacks: (graphic, kind) pairs that have some.
            Array<engine::PPtr<engine::Object>> callbackOwners[3];
        };

        Graphics& G()
        {
            static Graphics* g = new Graphics();
            return *g;
        }

        bool HasCallbacks(const Graphic* g, i32 kind)
        {
            for (const engine::PPtr<engine::Object>& o : G().callbackOwners[kind])
                if (o.Get() == g)
                    return true;
            return false;
        }

        void Dirty(Graphic* g, i32 kind)
        {
            if (HasCallbacks(g, kind))
                Graphic::dirtyCallback().Invoke(g, kind);
        }
    }

    Graphic::~Graphic() = default;


    engine::Material* Graphic::defaultGraphicMaterial() { return engine::Canvas::GetDefaultCanvasMaterial(); }

    void Graphic::SetColor(Color value)
    {
        if (color_ == value)
            return;
        color_ = value;
        SetVerticesDirty();
    }

    void Graphic::SetRaycastTarget(bool value)
    {
        if (value != raycastTarget_)
        {
            if (raycastTarget_)
                GraphicRegistry::UnregisterRaycastGraphicForCanvas(canvas(), this);
            raycastTarget_ = value;
            if (raycastTarget_ && isActiveAndEnabled())
                GraphicRegistry::RegisterRaycastGraphicForCanvas(canvas(), this);
        }
        raycastTargetCache_ = value;
    }

    i32 Graphic::depth() const
    {
        engine::CanvasRenderer* r = canvasRenderer();
        return r ? r->absoluteDepth() : -1;
    }

    engine::RectTransform* Graphic::rectTransform() const
    {
        engine::Transform* t = engine::Component::transform();
        return t && t->Is<engine::RectTransform>() ? static_cast<engine::RectTransform*>(t) : nullptr;
    }

    engine::Canvas* Graphic::canvas()
    {
        if (!canvas_.Get())
            CacheCanvas();
        return canvas_.Get();
    }

    engine::CanvasRenderer* Graphic::canvasRenderer() const { return ComponentOf<engine::CanvasRenderer>(this); }

    void Graphic::CacheCanvas()
    {
        canvas_ = engine::PPtr<engine::Canvas>();
        for (engine::Transform* t = engine::Component::transform(); t; t = t->parent())
            if (engine::Canvas* c = ComponentOf<engine::Canvas>(t->gameObject()); c && c->isActiveAndEnabled())
            {
                canvas_ = engine::PPtr<engine::Canvas>(c);
                return;
            }
    }

    engine::Material* Graphic::material() const { return material_.Get() ? material_.Get() : defaultMaterial(); }

    void Graphic::SetMaterial(engine::Material* value)
    {
        if (material_.Get() == value)
            return;
        material_ = engine::PPtr<engine::Material>(value);
        SetMaterialDirty();
    }

    engine::Material* Graphic::materialForRendering()
    {
        engine::Material* current = material();
        engine::GameObject* g = gameObject();
        if (!g)
            return current;
        for (u32 i = 0; i < g->GetComponentCount(); i++)
        {
            engine::Component* c = g->GetComponentAtIndex(i);
            UIBehaviour* b = AsUIBehaviour(c);
            if (IMaterialModifier* m = b ? b->AsMaterialModifier() : nullptr)
                current = m->GetModifiedMaterial(current);
            else if (ManagedCalls::IsManaged(c))
                current = ManagedCalls::ModifyMaterial(c, current);
        }
        return current;
    }

    engine::Texture* Graphic::mainTexture() const { return engine::Texture2D::whiteTexture(); }

    // ------------------------------------------------------------------------------------------------- dirtiness

    void Graphic::SetAllDirty()
    {
        if (skipLayoutUpdate_)
            skipLayoutUpdate_ = false;
        else
            SetLayoutDirty();
        if (skipMaterialUpdate_)
            skipMaterialUpdate_ = false;
        else
            SetMaterialDirty();
        SetVerticesDirty();
        SetRaycastDirty();
    }

    void Graphic::SetLayoutDirty()
    {
        if (!IsActive())
            return;
        LayoutRebuilder::MarkLayoutForRebuild(rectTransform());
        Dirty(this, 0);
    }

    void Graphic::SetVerticesDirty()
    {
        if (!IsActive())
            return;
        vertsDirty_ = true;
        CanvasUpdateRegistry::RegisterCanvasElementForGraphicRebuildNative(this);
        Dirty(this, 1);
    }

    void Graphic::SetMaterialDirty()
    {
        if (!IsActive())
            return;
        materialDirty_ = true;
        CanvasUpdateRegistry::RegisterCanvasElementForGraphicRebuildNative(this);
        Dirty(this, 2);
    }

    void Graphic::SetRaycastDirty()
    {
        if (raycastTargetCache_ != raycastTarget_)
        {
            if (raycastTarget_ && isActiveAndEnabled())
                GraphicRegistry::RegisterRaycastGraphicForCanvas(canvas(), this);
            else if (!raycastTarget_)
                GraphicRegistry::UnregisterRaycastGraphicForCanvas(canvas(), this);
        }
        raycastTargetCache_ = raycastTarget_;
    }

    void Graphic::RegisterDirtyCallbackInternal(i32 kind, bool add, u64 key)
    {
        (void)key;
        if (kind < 0 || kind > 2)
            return;
        Array<engine::PPtr<engine::Object>>& owners = G().callbackOwners[kind];
        for (usize i = 0; i < owners.Size(); i++)
            if (owners[i].Get() == this || !owners[i].Get())
            {
                if (owners[i].Get() == this && add)
                    return;
                owners.RemoveAt(i);
                i--;
            }
        if (add)
            owners.Push(engine::PPtr<engine::Object>(this));
    }

    engine::Event<Graphic*, i32>& Graphic::dirtyCallback()
    {
        static engine::Event<Graphic*, i32>* e = new engine::Event<Graphic*, i32>();
        return *e;
    }

    // ------------------------------------------------------------------------------------------------- rebuilding

    void Graphic::Rebuild(CanvasUpdate update)
    {
        engine::CanvasRenderer* r = canvasRenderer();
        if (!r || r->cull())
            return;
        if (update == CanvasUpdate::PreRender)
        {
            if (vertsDirty_)
            {
                UpdateGeometry();
                vertsDirty_ = false;
            }
            if (materialDirty_)
            {
                UpdateMaterial();
                materialDirty_ = false;
            }
        }
    }

    void Graphic::UpdateMaterial()
    {
        if (!IsActive())
            return;
        engine::CanvasRenderer* r = canvasRenderer();
        if (!r)
            return;
        r->SetMaterialCount(1);
        r->SetMaterial(materialForRendering(), 0);
        r->SetTexture(mainTexture());
    }

    engine::Mesh* Graphic::WorkerMesh()
    {
        Graphics& g = G();
        if (!g.worker.Get())
        {
            engine::Mesh* m = engine::Mesh::Create();
            m->SetName("Shared UI Mesh");
            m->hideFlags = engine::HideFlags::HideAndDontSave;
            g.worker = engine::PPtr<engine::Mesh>(m);
        }
        return g.worker.Get();
    }

    VertexHelper& Graphic::SharedVertexHelper()
    {
        Graphics& g = G();
        if (!g.helper)
            g.helper = new VertexHelper();
        return *g.helper;
    }

    void Graphic::UpdateGeometry() { DoMeshGeneration(); }

    void Graphic::DoMeshGeneration()
    {
        VertexHelper& vh = SharedVertexHelper();
        engine::RectTransform* rt = rectTransform();
        if (rt && rt->rect().width >= 0 && rt->rect().height >= 0)
            OnPopulateMesh(&vh);
        else
            vh.Clear();
        // The mesh modifiers of the GameObject, in their order.
        if (engine::GameObject* g = gameObject())
            for (u32 i = 0; i < g->GetComponentCount(); i++)
            {
                engine::Component* c = g->GetComponentAtIndex(i);
                UIBehaviour* b = AsUIBehaviour(c);
                if (IMeshModifier* m = b ? b->AsMeshModifier() : nullptr)
                    m->ModifyMesh(&vh);
                else if (ManagedCalls::IsManaged(c))
                    ManagedCalls::ModifyMesh(c, vh);
            }
        engine::Mesh* mesh = WorkerMesh();
        vh.FillMesh(mesh);
        if (engine::CanvasRenderer* r = canvasRenderer())
            r->SetMesh(mesh);
    }

    void Graphic::OnPopulateMesh(VertexHelper* vh)
    {
        Rect r = GetPixelAdjustedRect();
        Vector4 v(r.x, r.y, r.x + r.width, r.y + r.height);
        Color32 c = color_;
        vh->Clear();
        vh->AddVert(Vector3(v.x, v.y, 0), c, Vector4(0, 0, 0, 0));
        vh->AddVert(Vector3(v.x, v.w, 0), c, Vector4(0, 1, 0, 0));
        vh->AddVert(Vector3(v.z, v.w, 0), c, Vector4(1, 1, 0, 0));
        vh->AddVert(Vector3(v.z, v.y, 0), c, Vector4(1, 0, 0, 0));
        vh->AddTriangle(0, 1, 2);
        vh->AddTriangle(2, 3, 0);
    }

    void Graphic::OnRebuildRequested() { SetAllDirty(); }

    void Graphic::OnCullingChanged()
    {
        engine::CanvasRenderer* r = canvasRenderer();
        if (r && !r->cull() && (vertsDirty_ || materialDirty_))
            CanvasUpdateRegistry::RegisterCanvasElementForGraphicRebuildNative(this);
    }

    // ------------------------------------------------------------------------------------------------- raycasts

    bool Graphic::Raycast(Vector2 sp, engine::Camera* eventCamera) { return RaycastIgnoringMasks(sp, eventCamera, false); }

    bool Graphic::RaycastIgnoringMasks(Vector2 sp, engine::Camera* eventCamera, bool ignoreMasks)
    {
        if (!isActiveAndEnabled())
            return false;
        bool ignoreParentGroups = false, continueTraversal = true;
        for (engine::Transform* t = engine::Component::transform(); t; t = continueTraversal ? t->parent() : nullptr)
        {
            engine::GameObject* g = t->gameObject();
            for (u32 i = 0; g && i < g->GetComponentCount(); i++)
            {
                engine::Component* c = g->GetComponentAtIndex(i);
                if (!c)
                    continue;
                if (c->Is<engine::Canvas>() && static_cast<engine::Canvas*>(c)->overrideSorting())
                    continueTraversal = false;
                bool valid = true;
                if (c->Is<engine::CanvasGroup>())
                {
                    auto* group = static_cast<engine::CanvasGroup*>(c);
                    if (!group->enabled())
                        continue;
                    if (!ignoreParentGroups && group->ignoreParentGroups())
                    {
                        ignoreParentGroups = true;
                        valid = group->IsRaycastLocationValid(sp, eventCamera);
                    }
                    else if (!ignoreParentGroups)
                        valid = group->IsRaycastLocationValid(sp, eventCamera);
                }
                else if (ICanvasRaycastFilterNative* filter = AsUIBehaviour(c) ? AsUIBehaviour(c)->AsRaycastFilter() : nullptr)
                {
                    // The masks do not stop the raycasts of graphics that are not maskable.
                    if (ignoreMasks && (ComponentOf<Mask>(g) == c || ComponentOf<RectMask2D>(g) == c))
                        continue;
                    valid = filter->IsRaycastLocationValid(sp, eventCamera);
                }
                else if (ManagedCalls::IsManaged(c))
                {
                    bool implemented = false;
                    valid = ManagedCalls::RaycastFilter(c, sp, eventCamera, implemented);
                }
                else
                    continue;
                if (!valid)
                    return false;
            }
        }
        return true;
    }

    Vector2 Graphic::PixelAdjustPoint(Vector2 point)
    {
        engine::Canvas* c = canvas();
        if (!c || c->renderMode() == engine::RenderMode::WorldSpace || c->scaleFactor() == 0 || !c->pixelPerfect())
            return point;
        return engine::RectTransformUtility::PixelAdjustPoint(point, engine::Component::transform(), c);
    }

    Rect Graphic::GetPixelAdjustedRect()
    {
        engine::Canvas* c = canvas();
        engine::RectTransform* rt = rectTransform();
        if (!rt)
            return Rect(0, 0, 0, 0);
        if (!c || c->renderMode() == engine::RenderMode::WorldSpace || c->scaleFactor() == 0 || !c->pixelPerfect())
            return rt->rect();
        return engine::RectTransformUtility::PixelAdjustRect(rt, c);
    }

    // ------------------------------------------------------------------------------------------------- tweens

    void Graphic::CrossFadeColor(Color targetColor, f32 duration, bool ignoreTimeScale, bool useAlpha)
    {
        CrossFadeColor(targetColor, duration, ignoreTimeScale, useAlpha, true);
    }

    void Graphic::CrossFadeColor(Color targetColor, f32 duration, bool ignoreTimeScale, bool useAlpha, bool useRGB)
    {
        engine::CanvasRenderer* r = canvasRenderer();
        if (!r || (!useRGB && !useAlpha))
            return;
        Color current = r->GetColor();
        if (current == targetColor)
        {
            tween_.running = false;
            return;
        }
        Tween t;
        t.from = current;
        t.to = targetColor;
        t.duration = duration;
        t.ignoreTimeScale = ignoreTimeScale;
        t.alpha = useAlpha;
        t.rgb = useRGB;
        StartTween(t);
    }

    void Graphic::CrossFadeAlpha(f32 alpha, f32 duration, bool ignoreTimeScale)
    {
        CrossFadeColor(Color(1, 1, 1, alpha), duration, ignoreTimeScale, true, false);
    }

    namespace
    {
        // The tween's color at a share of its time: only the channels it changes.
        Color TweenColor(Color from, Color to, f32 share, bool alpha, bool rgb)
        {
            Color c = Color::Lerp(from, to, share);
            if (!rgb)
                c.r = from.r, c.g = from.g, c.b = from.b;
            if (!alpha)
                c.a = from.a;
            return c;
        }
    }

    void Graphic::StartTween(const Tween& t)
    {
        engine::CanvasRenderer* r = canvasRenderer();
        engine::GameObject* g = gameObject();
        if (!g || !g->activeInHierarchy())
        {
            // Not running on an inactive object: the end at once.
            tween_.running = false;
            if (r)
                r->SetColor(TweenColor(t.from, t.to, 1, t.alpha, t.rgb));
            return;
        }
        tween_ = t;
        tween_.running = true;
        tween_.elapsed = 0;
        Graphics& s = G();
        bool listed = false;
        for (const engine::PPtr<engine::Object>& o : s.tweening)
            listed = listed || o.Get() == this;
        if (!listed)
            s.tweening.Push(engine::PPtr<engine::Object>(this));
        if (!s.tweensHooked)
        {
            s.tweensHooked = true;
            engine::Canvas::preWillRenderCanvasesInternal().Add([](void*) { Graphic::UpdateTweens(); });
        }
        // The first step now (a coroutine's start runs to its first wait).
        tween_.elapsed += tween_.ignoreTimeScale ? engine::Time::unscaledDeltaTime() : engine::Time::deltaTime();
        f32 share = tween_.duration > 0 ? tween_.elapsed / tween_.duration : 1.0f;
        if (r)
            r->SetColor(TweenColor(tween_.from, tween_.to, share > 1 ? 1 : share, tween_.alpha, tween_.rgb));
    }

    void Graphic::UpdateTweens()
    {
        Graphics& s = G();
        for (usize i = 0; i < s.tweening.Size();)
        {
            auto* g = static_cast<Graphic*>(s.tweening[i].Get());
            if (!g || !g->tween_.running || !g->gameObject() || !g->gameObject()->activeInHierarchy())
            {
                if (g)
                    g->tween_.running = false;
                s.tweening.RemoveAt(i);
                continue;
            }
            Tween& t = g->tween_;
            engine::CanvasRenderer* r = g->canvasRenderer();
            if (t.elapsed < t.duration)
            {
                t.elapsed += t.ignoreTimeScale ? engine::Time::unscaledDeltaTime() : engine::Time::deltaTime();
                f32 share = t.duration > 0 ? t.elapsed / t.duration : 1.0f;
                if (r)
                    r->SetColor(TweenColor(t.from, t.to, share > 1 ? 1 : share, t.alpha, t.rgb));
                i++;
                continue;
            }
            if (r)
                r->SetColor(TweenColor(t.from, t.to, 1, t.alpha, t.rgb));
            t.running = false;
            s.tweening.RemoveAt(i);
        }
    }

    // ------------------------------------------------------------------------------------------------- messages

    void Graphic::OnEnable()
    {
        UIBehaviour::OnEnable();
        CacheCanvas();
        GraphicRegistry::RegisterGraphicForCanvas(canvas(), this);
        SetAllDirty();
    }

    void Graphic::OnDisable()
    {
        GraphicRebuildTracker::UnTrackGraphic(this);
        GraphicRegistry::DisableGraphicForCanvas(canvas_.Get(), this);
        CanvasUpdateRegistry::DisableCanvasElementForRebuildNative(this);
        if (engine::CanvasRenderer* r = canvasRenderer())
            r->Clear();
        LayoutRebuilder::MarkLayoutForRebuild(rectTransform());
        tween_.running = false;
        UIBehaviour::OnDisable();
    }

    void Graphic::OnDestroy()
    {
        GraphicRebuildTracker::UnTrackGraphic(this);
        GraphicRegistry::UnregisterGraphicForCanvas(canvas_.Get(), this);
        CanvasUpdateRegistry::UnRegisterCanvasElementForRebuildNative(this);
        UIBehaviour::OnDestroy();
    }

    void Graphic::OnRectTransformDimensionsChange()
    {
        engine::GameObject* g = gameObject();
        if (!g || !g->activeInHierarchy())
            return;
        // A change by the layout itself needs no layout again.
        if (CanvasUpdateRegistry::IsRebuildingLayout())
            SetVerticesDirty();
        else
        {
            SetVerticesDirty();
            SetLayoutDirty();
        }
    }

    void Graphic::OnBeforeTransformParentChanged()
    {
        GraphicRegistry::UnregisterGraphicForCanvas(canvas_.Get(), this);
        LayoutRebuilder::MarkLayoutForRebuild(rectTransform());
    }

    void Graphic::OnTransformParentChanged()
    {
        UIBehaviour::OnTransformParentChanged();
        canvas_ = engine::PPtr<engine::Canvas>();
        if (!IsActive())
            return;
        CacheCanvas();
        GraphicRegistry::RegisterGraphicForCanvas(canvas(), this);
        SetAllDirty();
    }

    void Graphic::OnCanvasHierarchyChanged()
    {
        engine::Canvas* current = canvas_.Get();
        canvas_ = engine::PPtr<engine::Canvas>();
        if (!IsActive())
        {
            GraphicRegistry::UnregisterGraphicForCanvas(current, this);
            return;
        }
        CacheCanvas();
        if (current != canvas_.Get())
        {
            GraphicRegistry::UnregisterGraphicForCanvas(current, this);
            if (IsActive())
                GraphicRegistry::RegisterGraphicForCanvas(canvas(), this);
        }
    }

    // ------------------------------------------------------------------------------------------------- registries

    namespace
    {
        struct CanvasGraphics
        {
            engine::PPtr<engine::Canvas> canvas;
            Array<engine::PPtr<engine::Object>> all, raycastable;
        };

        Array<CanvasGraphics>& Registry()
        {
            static Array<CanvasGraphics>* r = new Array<CanvasGraphics>();
            return *r;
        }

        CanvasGraphics* Of(engine::Canvas* c, bool make)
        {
            Array<CanvasGraphics>& r = Registry();
            for (usize i = 0; i < r.Size();)
            {
                if (!r[i].canvas.Get())
                {
                    r.RemoveAt(i);
                    continue;
                }
                if (r[i].canvas.Get() == c)
                    return &r[i];
                i++;
            }
            if (!make)
                return nullptr;
            CanvasGraphics e;
            e.canvas = engine::PPtr<engine::Canvas>(c);
            r.Push(e);
            return &r[r.Size() - 1];
        }

        void AddUnique(Array<engine::PPtr<engine::Object>>& list, Graphic* g)
        {
            for (const engine::PPtr<engine::Object>& o : list)
                if (o.Get() == g)
                    return;
            list.Push(engine::PPtr<engine::Object>(g));
        }

        void RemoveFrom(Array<engine::PPtr<engine::Object>>& list, Graphic* g)
        {
            for (usize i = 0; i < list.Size(); i++)
                if (list[i].Get() == g)
                {
                    list.RemoveAt(i);
                    return;
                }
        }

        Array<Graphic*> Live(const Array<engine::PPtr<engine::Object>>& list)
        {
            Array<Graphic*> out;
            for (const engine::PPtr<engine::Object>& o : list)
                if (engine::Object* x = o.Get())
                    out.Push(static_cast<Graphic*>(x));
            return out;
        }
    }

    void GraphicRegistry::RegisterGraphicForCanvas(engine::Canvas* c, Graphic* graphic)
    {
        if (!c || !graphic)
            return;
        AddUnique(Of(c, true)->all, graphic);
        RegisterRaycastGraphicForCanvas(c, graphic);
    }

    void GraphicRegistry::RegisterRaycastGraphicForCanvas(engine::Canvas* c, Graphic* graphic)
    {
        if (!c || !graphic || !graphic->raycastTarget())
            return;
        AddUnique(Of(c, true)->raycastable, graphic);
    }

    void GraphicRegistry::UnregisterGraphicForCanvas(engine::Canvas* c, Graphic* graphic)
    {
        if (!c || !graphic)
            return;
        if (CanvasGraphics* e = Of(c, false))
        {
            RemoveFrom(e->all, graphic);
            RemoveFrom(e->raycastable, graphic);
        }
    }

    void GraphicRegistry::UnregisterRaycastGraphicForCanvas(engine::Canvas* c, Graphic* graphic)
    {
        if (!c || !graphic)
            return;
        if (CanvasGraphics* e = Of(c, false))
            RemoveFrom(e->raycastable, graphic);
    }

    void GraphicRegistry::DisableGraphicForCanvas(engine::Canvas* c, Graphic* graphic) { UnregisterGraphicForCanvas(c, graphic); }
    void GraphicRegistry::DisableRaycastGraphicForCanvas(engine::Canvas* c, Graphic* graphic) { UnregisterRaycastGraphicForCanvas(c, graphic); }

    Array<Graphic*> GraphicRegistry::GetGraphicsForCanvasInternal(engine::Canvas* canvas)
    {
        CanvasGraphics* e = canvas ? Of(canvas, false) : nullptr;
        return e ? Live(e->all) : Array<Graphic*>();
    }

    Array<Graphic*> GraphicRegistry::GetRaycastableGraphicsForCanvasInternal(engine::Canvas* canvas)
    {
        CanvasGraphics* e = canvas ? Of(canvas, false) : nullptr;
        return e ? Live(e->raycastable) : Array<Graphic*>();
    }

    namespace
    {
        Array<engine::PPtr<engine::Object>>& Tracked()
        {
            static Array<engine::PPtr<engine::Object>>* t = new Array<engine::PPtr<engine::Object>>();
            return *t;
        }
    }

    void GraphicRebuildTracker::TrackGraphic(Graphic* g)
    {
        if (g)
            AddUnique(Tracked(), g);
    }

    void GraphicRebuildTracker::UnTrackGraphic(Graphic* g) { RemoveFrom(Tracked(), g); }
    void GraphicRebuildTracker::DisableTrackGraphic(Graphic* g) { RemoveFrom(Tracked(), g); }

    void GraphicRebuildTracker::RebuildAll()
    {
        for (Graphic* g : Live(Tracked()))
            g->SetVerticesDirty();
    }

    void Graphic::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        canvas_ = engine::PPtr<engine::Canvas>();
    }

    void MaskableGraphic::OnAfterCloneNative()
    {
        Graphic::OnAfterCloneNative();
        // The stencil material and the masks are the original's.
        maskMaterial_ = engine::PPtr<engine::Material>();
        parentMask_ = engine::PPtr<engine::Object>();
        shouldRecalculateStencil_ = true;
        shouldRecalculate_ = true;
    }
}
