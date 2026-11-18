#include <Helix/CanvasUI/Core/CanvasUpdateRegistry.h>

#include <Helix/CanvasUI/Masking/Clipping.h>
#include <Helix/CanvasUI/Core/Implementation/CanvasUILog.h>
#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Object/Implementation/ObjectAccess.h>
#include <Helix/Engine/Object/Object.h>
#include <Helix/Engine/Script/ScriptClass.h>
#include <Helix/Engine/Transform/Transform.h>

namespace helix::canvasui
{
    bool UIBehaviour::IsDestroyed() const { return engine::ObjectAccess::Destroying(this); }

    UIBehaviour* AsUIBehaviour(engine::Component* c)
    {
        if (!c || !c->Is<engine::GameBehaviour>())
            return nullptr;
        const engine::ScriptClass* s = static_cast<engine::GameBehaviour*>(c)->script();
        return s && scripting::IsAssignableFrom(scripting::TypeOf<UIBehaviour>(), s->type) ? static_cast<UIBehaviour*>(c) : nullptr;
    }

    void UIBehavioursOf(const engine::GameObject* g, Array<UIBehaviour*>& out)
    {
        out.Clear();
        for (u32 i = 0; g && i < g->GetComponentCount(); i++)
            if (UIBehaviour* b = AsUIBehaviour(g->GetComponentAtIndex(i)))
                out.Push(b);
    }

    namespace
    {
        // A queued element: a native one, or a C# one by its key; its transform orders the layout queue.
        struct Entry
        {
            ICanvasElement* native = nullptr;
            // The component a native element is (gone: the element is never called).
            engine::PPtr<engine::Object> object;
            bool hasObject = false;
            u64 key = 0;
            engine::PPtr<engine::Transform> transform;
            bool disabled = false;

            bool Same(const Entry& o) const { return native ? native == o.native : (!o.native && key == o.key); }
        };

        struct Registry
        {
            Array<Entry> layout, graphic;
            bool performingLayout = false, performingGraphic = false;
            bool hooked = false;
            bool managedDestroyed = false;
        };

        Registry& R()
        {
            static Registry* r = new Registry();
            return *r;
        }

        void Hook()
        {
            Registry& r = R();
            if (r.hooked)
                return;
            r.hooked = true;
            engine::Canvas::willRenderCanvasesInternal().Add([](void*) { CanvasUpdateRegistry::PerformUpdate(); });
        }

        void Managed(u64 key, i32 what) { CanvasUpdateRegistry::managedRebuild().Invoke(key, what); }

        bool Destroyed(const Entry& e)
        {
            if (e.native)
                return (e.hasObject && !e.object.Get()) || e.native->IsDestroyed();
            R().managedDestroyed = true;
            Managed(e.key, 12);
            return R().managedDestroyed;
        }

        void Rebuild(const Entry& e, CanvasUpdate stage)
        {
            if (e.native)
                e.native->Rebuild(stage);
            else
                Managed(e.key, i32(stage));
        }

        void Complete(const Entry& e, bool layout)
        {
            if (e.native)
                layout ? e.native->LayoutComplete() : e.native->GraphicUpdateComplete();
            else
                Managed(e.key, layout ? 10 : 11);
        }

        i32 Find(const Array<Entry>& queue, const Entry& e)
        {
            for (usize i = 0; i < queue.Size(); i++)
                if (queue[i].Same(e))
                    return i32(i);
            return -1;
        }

        // 0 refused, 1 added, 2 there already (enabled again).
        i32 Add(Array<Entry>& queue, bool performing, const Entry& e, StringView what)
        {
            Hook();
            if (performing)
            {
                HX_LOG_ERROR(CanvasUILog, "Trying to add an element for {} rebuild while we are already inside a {} rebuild loop. This is not supported.", what, what);
                return 0;
            }
            if (i32 i = Find(queue, e); i >= 0)
            {
                queue[u32(i)].disabled = false;
                return 2;
            }
            queue.Push(e);
            return 1;
        }

        bool Remove(const Entry& e, bool disable)
        {
            Registry& r = R();
            if (r.performingLayout || r.performingGraphic)
            {
                HX_LOG_ERROR(CanvasUILog, "Trying to remove an element from the rebuild list while we are already inside a rebuild loop. This is not supported.");
                return false;
            }
            for (int pass = 0; pass < 2; pass++)
            {
                Array<Entry>& queue = pass == 0 ? r.layout : r.graphic;
                if (i32 i = Find(queue, e); i >= 0)
                {
                    Entry found = queue[u32(i)];
                    Complete(found, pass == 0);
                    if (disable)
                        queue[u32(i)].disabled = true;
                    else
                        queue.RemoveAt(u32(i));
                }
            }
            return true;
        }

        Entry Native(ICanvasElement* element)
        {
            Entry e;
            e.native = element;
            if (engine::Object* o = element ? element->ElementObject() : nullptr)
                e.object = engine::PPtr<engine::Object>(o), e.hasObject = true;
            e.transform = engine::PPtr<engine::Transform>(element ? element->transform() : nullptr);
            return e;
        }

        u32 ParentCount(const Entry& e)
        {
            u32 n = 0;
            if (engine::Transform* t = e.transform.Get())
                for (engine::Transform* p = t->parent(); p; p = p->parent())
                    n++;
            return n;
        }

        // Drops the elements that are gone (their completion not called: they cannot hear it).
        void Clean(Array<Entry>& queue)
        {
            for (usize i = queue.Size(); i-- > 0;)
                if (queue[i].disabled || Destroyed(queue[i]))
                    queue.RemoveAt(i);
        }
    }

    void CanvasUpdateRegistry::RegisterCanvasElementForLayoutRebuildNative(ICanvasElement* element) { TryRegisterCanvasElementForLayoutRebuildNative(element); }

    bool CanvasUpdateRegistry::TryRegisterCanvasElementForLayoutRebuildNative(ICanvasElement* element)
    {
        return element && Add(R().layout, R().performingLayout, Native(element), "layout") == 1;
    }

    void CanvasUpdateRegistry::RegisterCanvasElementForGraphicRebuildNative(ICanvasElement* element) { TryRegisterCanvasElementForGraphicRebuildNative(element); }

    bool CanvasUpdateRegistry::TryRegisterCanvasElementForGraphicRebuildNative(ICanvasElement* element)
    {
        return element && Add(R().graphic, R().performingGraphic, Native(element), "graphic") == 1;
    }

    void CanvasUpdateRegistry::UnRegisterCanvasElementForRebuildNative(ICanvasElement* element)
    {
        if (element)
            Remove(Native(element), false);
    }

    void CanvasUpdateRegistry::DisableCanvasElementForRebuildNative(ICanvasElement* element)
    {
        if (element)
            Remove(Native(element), true);
    }

    bool CanvasUpdateRegistry::IsRebuildingLayout() { return R().performingLayout; }
    bool CanvasUpdateRegistry::IsRebuildingGraphics() { return R().performingGraphic; }

    i32 CanvasUpdateRegistry::RegisterManaged(u64 key, engine::Transform* transform, i32 queue)
    {
        Entry e;
        e.key = key;
        e.transform = engine::PPtr<engine::Transform>(transform);
        Registry& r = R();
        return queue == 0 ? Add(r.layout, r.performingLayout, e, "layout") : Add(r.graphic, r.performingGraphic, e, "graphic");
    }

    bool CanvasUpdateRegistry::UnregisterManaged(u64 key, bool disable)
    {
        Entry e;
        e.key = key;
        return Remove(e, disable);
    }

    engine::Event<u64, i32>& CanvasUpdateRegistry::managedRebuild()
    {
        static engine::Event<u64, i32>* e = new engine::Event<u64, i32>();
        return *e;
    }

    void CanvasUpdateRegistry::SetManagedDestroyed(bool destroyed) { R().managedDestroyed = destroyed; }

    void CanvasUpdateRegistry::PerformUpdate()
    {
        Registry& r = R();
        engine::UISystemProfilerApi::BeginSample(engine::UISystemProfilerApi::SampleType::Layout);
        Clean(r.layout);
        r.performingLayout = true;
        // Parents before their children (a stable sort keeps the order of registration among equals).
        Array<u32> depth;
        for (const Entry& e : r.layout)
            depth.Push(ParentCount(e));
        for (usize i = 1; i < r.layout.Size(); i++)
            for (usize j = i; j > 0 && depth[j - 1] > depth[j]; j--)
            {
                std::swap(r.layout[j - 1], r.layout[j]);
                std::swap(depth[j - 1], depth[j]);
            }
        for (i32 stage = i32(CanvasUpdate::Prelayout); stage <= i32(CanvasUpdate::PostLayout); stage++)
            for (usize i = 0; i < r.layout.Size(); i++)
            {
                Entry e = r.layout[i];
                if (!e.disabled && !Destroyed(e))
                    Rebuild(e, CanvasUpdate(stage));
            }
        for (usize i = 0; i < r.layout.Size(); i++)
            Complete(r.layout[i], true);
        r.layout.Clear();
        r.performingLayout = false;
        engine::UISystemProfilerApi::EndSample(engine::UISystemProfilerApi::SampleType::Layout);

        engine::UISystemProfilerApi::BeginSample(engine::UISystemProfilerApi::SampleType::Render);
        r.performingGraphic = true;
        // The layout is done: the clippers cut their children now.
        ClipperRegistry::Cull();
        Clean(r.graphic);
        for (i32 stage = i32(CanvasUpdate::PreRender); stage < i32(CanvasUpdate::MaxUpdateValue); stage++)
            for (usize i = 0; i < r.graphic.Size(); i++)
            {
                Entry e = r.graphic[i];
                if (!e.disabled && !Destroyed(e))
                    Rebuild(e, CanvasUpdate(stage));
            }
        for (usize i = 0; i < r.graphic.Size(); i++)
            Complete(r.graphic[i], false);
        r.graphic.Clear();
        r.performingGraphic = false;
        engine::UISystemProfilerApi::EndSample(engine::UISystemProfilerApi::SampleType::Render);
    }
}
