#include <Helix/CanvasUI/Events/EventSystem.h>

#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Transform/RectTransformUtility.h>

#include <cmath>
#include <limits>
#include <utility>

namespace helix::canvasui
{
    // --------------------------------------------------------------------------------------------- GraphicRaycaster

    engine::Canvas* GraphicRaycaster::CanvasOf() const { return ComponentOf<engine::Canvas>(gameObject()); }

    engine::Camera* GraphicRaycaster::eventCamera() const
    {
        engine::Canvas* c = CanvasOf();
        if (!c)
            return nullptr;
        engine::RenderMode mode = c->renderMode();
        if (mode == engine::RenderMode::ScreenSpaceOverlay || (mode == engine::RenderMode::ScreenSpaceCamera && !c->worldCamera()))
            return nullptr;
        return c->worldCamera() ? c->worldCamera() : engine::Camera::main();
    }

    i32 GraphicRaycaster::sortOrderPriority() const
    {
        engine::Canvas* c = CanvasOf();
        return c && c->renderMode() == engine::RenderMode::ScreenSpaceOverlay ? c->sortingOrder() : BaseRaycaster::sortOrderPriority();
    }

    i32 GraphicRaycaster::renderOrderPriority() const
    {
        engine::Canvas* c = CanvasOf();
        return c && c->renderMode() == engine::RenderMode::ScreenSpaceOverlay ? c->rootCanvas()->renderOrder() : BaseRaycaster::renderOrderPriority();
    }

    void GraphicRaycaster::RaycastInternal(PointerEventData* eventData)
    {
        engine::Canvas* canvas = CanvasOf();
        if (!canvas)
            return;
        Array<Graphic*> graphics = GraphicRegistry::GetRaycastableGraphicsForCanvasInternal(canvas);
        if (graphics.Empty())
            return;
        engine::Camera* camera = eventCamera();
        i32 displayIndex = canvas->renderMode() == engine::RenderMode::ScreenSpaceOverlay || !camera ? canvas->targetDisplay() : camera->targetDisplay();
        Vector2 position = eventData->position();
        // One display: the events of others are not this canvas's.
        if (displayIndex != eventData->displayIndex() && eventData->displayIndex() != 0)
            return;
        // Outside the camera's view (the screen's without a camera): nothing.
        Vector2 view;
        if (!camera)
            view = Vector2(position.x / f32(engine::Screen::width()), position.y / f32(engine::Screen::height()));
        else
        {
            Vector3 v = camera->ScreenToViewportPoint(Vector3(position.x, position.y, 0));
            view = Vector2(v.x, v.y);
        }
        if (view.x < 0 || view.x > 1 || view.y < 0 || view.y > 1)
            return;
        f32 hitDistance = std::numeric_limits<f32>::max();
        Ray ray;
        if (camera)
            ray = camera->ScreenPointToRay(Vector3(position.x, position.y, 0));
        // Blocking objects of the physics scenes (2D and 3D) come with the physics step: nothing blocks yet.
        // The graphics under the point: drawn, not culled, inside their rectangles (less their padding), before the far
        // plane, passing their filters; the deepest first.
        Array<Graphic*> hits;
        for (Graphic* g : graphics)
        {
            engine::CanvasRenderer* r = g->canvasRenderer();
            if (!g->raycastTarget() || !r || r->cull() || g->depth() == -1)
                continue;
            if (!engine::RectTransformUtility::RectangleContainsScreenPoint(g->rectTransform(), position, camera, g->raycastPadding()))
                continue;
            if (camera && camera->WorldToScreenPoint(g->rectTransform()->position()).z > camera->farClipPlane())
                continue;
            if (g->Raycast(position, camera))
                hits.Push(g);
        }
        for (usize i = 1; i < hits.Size(); i++)
            for (usize j = i; j > 0 && hits[j]->depth() > hits[j - 1]->depth(); j--)
                std::swap(hits[j - 1], hits[j]);
        Array<RaycastResult>& out = Results();
        for (Graphic* g : hits)
        {
            engine::Transform* t = g->engine::Component::transform();
            Vector3 forward = t->rotation() * Vector3(0, 0, 1);
            bool append = true;
            if (ignoreReversedGraphics_)
            {
                if (!camera)
                    // Without a camera the canvas faces the screen: a graphic turned away from it is reversed.
                    append = Vector3::Dot(Vector3(0, 0, 1), forward) > 0;
                else
                {
                    engine::Transform* ct = camera->transform();
                    Vector3 cameraForward = ct->rotation() * Vector3(0, 0, 1) * camera->nearClipPlane();
                    append = Vector3::Dot(t->position() - ct->position() - cameraForward, forward) >= 0;
                }
            }
            if (!append)
                continue;
            f32 distance = 0;
            if (camera && canvas->renderMode() != engine::RenderMode::ScreenSpaceOverlay)
            {
                // The ray's distance to the graphic's plane; behind the camera: not hit.
                distance = Vector3::Dot(forward, t->position() - ray.origin) / Vector3::Dot(forward, ray.direction);
                if (distance < 0)
                    continue;
            }
            if (distance >= hitDistance)
                continue;
            RaycastResult r;
            r.gameObject = engine::PPtr<engine::GameObject>(g->gameObject());
            r.module = engine::PPtr<engine::Object>(this);
            r.distance = distance;
            r.screenPosition = position;
            r.displayIndex = displayIndex;
            r.index = f32(out.Size());
            r.depth = g->depth();
            r.sortingLayer = canvas->sortingLayerID();
            r.sortingOrder = canvas->sortingOrder();
            r.worldPosition = ray.origin + ray.direction * distance;
            r.worldNormal = Vector3(-forward.x, -forward.y, -forward.z);
            out.Push(r);
        }
    }

    // -------------------------------------------------------------------------------------------- PhysicsRaycaster

    engine::Camera* PhysicsRaycaster::eventCamera() const
    {
        engine::Camera* own = ComponentOf<engine::Camera>(gameObject());
        return own ? own : engine::Camera::main();
    }

    i32 PhysicsRaycaster::depth() const
    {
        engine::Camera* c = eventCamera();
        return c ? i32(c->depth()) : 0xFFFFFF;
    }

    i32 PhysicsRaycaster::finalEventMask() const
    {
        engine::Camera* c = eventCamera();
        return c ? i32(c->cullingMask()) & eventMask_ : kNoEventMaskSet;
    }

    bool PhysicsRaycaster::ComputeRayAndDistance(PointerEventData* eventData, Ray& ray, i32& eventDisplayIndex, f32& distanceToClipPlane)
    {
        engine::Camera* c = eventCamera();
        if (!c)
            return false;
        eventDisplayIndex = eventData->displayIndex();
        Vector2 p = eventData->position();
        Rect pixels = c->pixelRect();
        if (p.x < pixels.xMin() || p.x >= pixels.xMax() || p.y < pixels.yMin() || p.y >= pixels.yMax())
            return false;
        ray = c->ScreenPointToRay(Vector3(p.x, p.y, 0));
        f32 z = ray.direction.z;
        distanceToClipPlane = std::fabs(z) < 1e-6f ? std::numeric_limits<f32>::infinity() : std::fabs((c->farClipPlane() - c->nearClipPlane()) / z);
        return true;
    }

    // The physics scenes' hits come with the physics step: no colliders yet, nothing hit.
    void PhysicsRaycaster::RaycastInternal(PointerEventData* eventData) { (void)eventData; }
    void Physics2DRaycaster::RaycastInternal(PointerEventData* eventData) { (void)eventData; }

    // ----------------------------------------------------------------------------------------------- EventTrigger

    namespace
    {
        // Each entry index's storage and raise (function pointers have no index of their own).
        template <i32 I>
        engine::HelixEventStorage* EntryStorageAt(engine::Object* o)
        {
            return static_cast<EventTrigger*>(o)->EntryStorage(I);
        }

        template <i32 I>
        void EntryRaiseAt(engine::Object* o, Span<const scripting::Variant>)
        {
            static_cast<EventTrigger*>(o)->RaiseEntry(I, nullptr);
        }

        template <std::size_t... I>
        void RegisterEntries(std::index_sequence<I...>)
        {
            ((engine::RegisterNativeEvent(scripting::TypeOf<EventTrigger>(), i32(I), engine::NativeEventEntry{&EntryStorageAt<i32(I)>, &EntryRaiseAt<i32(I)>})), ...);
        }
    }

    EventTrigger::EventTrigger()
    {
        // The entries' callbacks are the trigger's native events by their index.
        static bool registered = false;
        if (!registered)
        {
            registered = true;
            RegisterEntries(std::make_index_sequence<256>());
        }
    }

    void EventTrigger::RaiseEntry(i32 index, BaseEventData* eventData)
    {
        if (index < 0 || usize(index) >= triggers_.entries.Size())
            return;
        triggers_.entries[usize(index)].callback.Invoke(eventData);
        entryRaised_.Invoke(index, eventData);
    }

    engine::HelixEventStorage* EventTrigger::EntryStorage(i32 index)
    {
        return index >= 0 && usize(index) < triggers_.entries.Size() ? &triggers_.entries[usize(index)].callback : nullptr;
    }

    bool EventTrigger::Handles(EventKind kind) const
    {
        // A trigger takes every event (as the reference's, which implements every handler).
        return i32(kind) >= 0 && kind < EventKind::Count;
    }

    void EventTrigger::HandleEvent(EventKind kind, BaseEventData* data)
    {
        switch (kind)
        {
        case EventKind::PointerEnter: OnPointerEnter(static_cast<PointerEventData*>(data)); break;
        case EventKind::PointerExit: OnPointerExit(static_cast<PointerEventData*>(data)); break;
        case EventKind::PointerMove: break;
        case EventKind::PointerDown: OnPointerDown(static_cast<PointerEventData*>(data)); break;
        case EventKind::PointerUp: OnPointerUp(static_cast<PointerEventData*>(data)); break;
        case EventKind::PointerClick: OnPointerClick(static_cast<PointerEventData*>(data)); break;
        case EventKind::InitializePotentialDrag: OnInitializePotentialDrag(static_cast<PointerEventData*>(data)); break;
        case EventKind::BeginDrag: OnBeginDrag(static_cast<PointerEventData*>(data)); break;
        case EventKind::Drag: OnDrag(static_cast<PointerEventData*>(data)); break;
        case EventKind::EndDrag: OnEndDrag(static_cast<PointerEventData*>(data)); break;
        case EventKind::Drop: OnDrop(static_cast<PointerEventData*>(data)); break;
        case EventKind::Scroll: OnScroll(static_cast<PointerEventData*>(data)); break;
        case EventKind::UpdateSelected: OnUpdateSelected(data); break;
        case EventKind::Select: OnSelect(data); break;
        case EventKind::Deselect: OnDeselect(data); break;
        case EventKind::Move: OnMove(static_cast<AxisEventData*>(data)); break;
        case EventKind::Submit: OnSubmit(data); break;
        case EventKind::Cancel: OnCancel(data); break;
        case EventKind::Count: break;
        }
    }

    EventTriggerType EventTrigger::EntryType(i32 index) const
    {
        return index >= 0 && usize(index) < triggers_.entries.Size() ? triggers_.entries[usize(index)].eventID : EventTriggerType::PointerEnter;
    }

    void EventTrigger::SetEntryType(i32 index, EventTriggerType type)
    {
        if (index >= 0 && usize(index) < triggers_.entries.Size())
            triggers_.entries[usize(index)].eventID = type;
    }

    void EventTrigger::AddEntry(EventTriggerType type)
    {
        EventTriggerEntry e;
        e.eventID = type;
        triggers_.entries.Push(std::move(e));
    }

    void EventTrigger::RemoveEntry(i32 index)
    {
        if (index >= 0 && usize(index) < triggers_.entries.Size())
            triggers_.entries.RemoveAt(usize(index));
    }

    void EventTrigger::Run(EventTriggerType id, BaseEventData* eventData)
    {
        for (usize i = 0; i < triggers_.entries.Size(); i++)
            if (triggers_.entries[i].eventID == id)
                // The entry's persistent calls, its C++ listeners, then C#'s.
                RaiseEntry(i32(i), eventData);
    }
}
