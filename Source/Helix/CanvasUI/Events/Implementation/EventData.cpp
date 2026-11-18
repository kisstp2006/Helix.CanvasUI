#include <Helix/CanvasUI/Events/EventSystem.h>

#include <Helix/CanvasUI/Core/Implementation/CanvasUILog.h>
#include <Helix/CanvasUI/Core/ManagedCalls.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Transform/Transform.h>

namespace helix::canvasui
{
    // ------------------------------------------------------------------------------------------------ RaycastResult

    bool RaycastResult::isValid() const
    {
        auto* m = static_cast<BaseRaycaster*>(module.Get());
        return m && gameObject.Get() && m->IsActive();
    }

    RaycastResultData RaycastResultData::From(const RaycastResult& r)
    {
        RaycastResultData d;
        d.gameObject = r.gameObject.Get() ? r.gameObject.Get()->GetEntityId() : engine::EntityId();
        d.module = r.module.Get() ? r.module.Get()->GetEntityId() : engine::EntityId();
        d.distance = r.distance, d.index = r.index, d.depth = r.depth, d.sortingGroupID = r.sortingGroupID;
        d.sortingGroupOrder = r.sortingGroupOrder, d.sortingLayer = r.sortingLayer, d.sortingOrder = r.sortingOrder;
        d.origin = r.origin, d.worldPosition = r.worldPosition, d.worldNormal = r.worldNormal, d.screenPosition = r.screenPosition;
        d.displayIndex = r.displayIndex;
        return d;
    }

    RaycastResult RaycastResultData::ToResult() const
    {
        RaycastResult r;
        r.gameObject = engine::PPtr<engine::GameObject>(engine::EntityIdToObject<engine::GameObject>(gameObject));
        r.module = engine::PPtr<engine::Object>(engine::EntityIdToObject(module));
        r.distance = distance, r.index = index, r.depth = depth, r.sortingGroupID = sortingGroupID, r.sortingGroupOrder = sortingGroupOrder;
        r.sortingLayer = sortingLayer, r.sortingOrder = sortingOrder, r.origin = origin, r.worldPosition = worldPosition, r.worldNormal = worldNormal;
        r.screenPosition = screenPosition, r.displayIndex = displayIndex;
        return r;
    }

    // ------------------------------------------------------------------------------------------------ event data

    BaseEventData::BaseEventData(EventSystem* eventSystem) : eventSystem_(eventSystem) {}

    EventSystem* BaseEventData::eventSystem() const { return static_cast<EventSystem*>(eventSystem_.Get()); }

    InputModule* BaseEventData::currentInputModule() const
    {
        EventSystem* s = eventSystem();
        return s ? s->currentInputModule() : nullptr;
    }

    engine::GameObject* BaseEventData::selectedObject() const
    {
        EventSystem* s = eventSystem();
        return s ? s->currentSelectedGameObject() : nullptr;
    }

    void BaseEventData::SetSelectedObject(engine::GameObject* value)
    {
        if (EventSystem* s = eventSystem())
            s->SetSelectedGameObject(value, this);
    }

    PointerEventData::PointerEventData(EventSystem* eventSystem) : BaseEventData(eventSystem) {}

    Array<engine::GameObject*> PointerEventData::HoveredInternal() const
    {
        Array<engine::GameObject*> out;
        for (const engine::PPtr<engine::GameObject>& g : hovered_)
            out.Push(g.Get());
        return out;
    }

    void PointerEventData::SetHoveredInternal(Span<engine::GameObject* const> value)
    {
        hovered_.Clear();
        for (engine::GameObject* g : value)
            hovered_.Push(engine::PPtr<engine::GameObject>(g));
    }

    void PointerEventData::SetPointerPress(engine::GameObject* value)
    {
        if (pointerPress_.Get() == value)
            return;
        lastPress_ = pointerPress_;
        pointerPress_ = engine::PPtr<engine::GameObject>(value);
    }

    engine::Camera* PointerEventData::enterEventCamera() const
    {
        auto* m = static_cast<BaseRaycaster*>(pointerCurrentRaycast.module.Get());
        return m ? m->eventCamera() : nullptr;
    }

    engine::Camera* PointerEventData::pressEventCamera() const
    {
        auto* m = static_cast<BaseRaycaster*>(pointerPressRaycast.module.Get());
        return m ? m->eventCamera() : nullptr;
    }

    String PointerEventData::Describe() const
    {
        auto name = [](const engine::PPtr<engine::GameObject>& g) { return g.Get() ? String(g.Get()->Name()) : String("null"); };
        String s;
        s += Format("<b>Position</b>: ({:.1f}, {:.1f})\n<b>delta</b>: ({:.1f}, {:.1f})\n<b>eligibleForClick</b>: {}\n", position_.x, position_.y, delta_.x,
                    delta_.y, eligibleForClick_ ? "True" : "False");
        s += Format("<b>pointerEnter</b>: {}\n<b>pointerPress</b>: {}\n<b>lastPointerPress</b>: {}\n<b>pointerDrag</b>: {}\n", name(pointerEnter_),
                    name(pointerPress_), name(lastPress_), name(pointerDrag_));
        s += Format("<b>Use Drag Threshold</b>: {}\n<b>Current Raycast:</b>\n{}\n<b>Press Raycast:</b>\n{}\n", useDragThreshold_ ? "True" : "False",
                    name(pointerCurrentRaycast.gameObject), name(pointerPressRaycast.gameObject));
        s += Format("<b>pressure</b>: {:.4f}\n<b>tangentialPressure</b>: {:.4f}\n<b>altitudeAngle</b>: {:.4f}\n<b>azimuthAngle</b>: {:.4f}\n<b>twist</b>: {:.4f}\n",
                    pressure_, tangentialPressure_, altitudeAngle_, azimuthAngle_, twist_);
        s += Format("<b>tilt</b>: ({:.4f}, {:.4f})\n<b>penStatus</b>: {}\n<b>radius</b>: ({:.4f}, {:.4f})\n<b>radiusVariance</b>: ({:.4f}, {:.4f})\n", tilt_.x, tilt_.y,
                    i32(penStatus_), radius_.x, radius_.y, radiusVariance_.x, radiusVariance_.y);
        return s;
    }

    // ------------------------------------------------------------------------------------------------ ExecuteEvents

    namespace
    {
        bool gHandled = false;

        bool Enabled(engine::Component* c) { return !c->Is<engine::Behaviour>() || static_cast<engine::Behaviour*>(c)->isActiveAndEnabled(); }

        // Calls (or only counts, with data null) the handlers of the GameObject; how many there are.
        u32 Run(engine::GameObject* target, BaseEventData* data, EventKind kind)
        {
            if (!target || !target->activeInHierarchy())
                return 0;
            // By id: a handler may destroy components.
            Array<engine::EntityId> ids;
            for (u32 i = 0; i < target->GetComponentCount(); i++)
                if (engine::Component* c = target->GetComponentAtIndex(i))
                    ids.Push(c->GetEntityId());
            u32 count = 0;
            for (engine::EntityId id : ids)
            {
                auto* c = engine::EntityIdToObject<engine::Component>(id);
                if (!c || !Enabled(c))
                    continue;
                UIBehaviour* b = AsUIBehaviour(c);
                INativeEventHandler* h = b ? b->AsEventHandler() : nullptr;
                if (h && h->Handles(kind))
                {
                    count++;
                    if (data)
                        h->HandleEvent(kind, data);
                    continue;
                }
                if (!ManagedCalls::IsManaged(c))
                    continue;
                gHandled = false;
                ExecuteEvents::managedExecute().Invoke(c, data ? i32(kind) : -1 - i32(kind), data);
                if (gHandled)
                    count++;
            }
            return count;
        }
    }

    engine::Event<engine::Component*, i32, BaseEventData*>& ExecuteEvents::managedExecute()
    {
        static engine::Event<engine::Component*, i32, BaseEventData*>* e = new engine::Event<engine::Component*, i32, BaseEventData*>();
        return *e;
    }

    void ExecuteEvents::SetHandled(bool value) { gHandled = value; }

    bool ExecuteEvents::ExecuteInternal(engine::GameObject* target, BaseEventData* eventData, EventKind kind)
    {
        return eventData && Run(target, eventData, kind) > 0;
    }

    engine::GameObject* ExecuteEvents::ExecuteHierarchyInternal(engine::GameObject* root, BaseEventData* eventData, EventKind kind)
    {
        for (engine::Transform* t = root ? root->transform() : nullptr; t; t = t->parent())
            if (ExecuteInternal(t->gameObject(), eventData, kind))
                return t->gameObject();
        return nullptr;
    }

    bool ExecuteEvents::CanHandleEventInternal(engine::GameObject* go, EventKind kind) { return Run(go, nullptr, kind) > 0; }

    engine::GameObject* ExecuteEvents::GetEventHandlerInternal(engine::GameObject* root, EventKind kind)
    {
        for (engine::Transform* t = root ? root->transform() : nullptr; t; t = t->parent())
            if (CanHandleEventInternal(t->gameObject(), kind))
                return t->gameObject();
        return nullptr;
    }
}
