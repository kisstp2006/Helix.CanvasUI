#include <Helix/CanvasUI/Events/PanelEvents.h>

#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/UI/Engine/PanelInput.h>
#include <Helix/UI/Engine/PanelInputConfiguration.h>
#include <Helix/UI/Events/EventSystem.h>

#include <limits>

namespace helix::canvasui
{
    namespace
    {
        using ui::PanelInput;

        // The screen's point from the top left of an event's position (bottom left).
        Vector2 FromBottom(Vector2 p) { return Vector2(p.x, f32(engine::Screen::height()) - p.y); }

        f64 Now() { return engine::Time::realtimeSinceStartupAsDouble(); }

        i32 UiPointerId(PointerEventData* e)
        {
            InputModule* m = e->currentInputModule();
            return m ? m->ConvertUIToolkitPointerId(e) : (e->pointerId() < 0 ? ui::PointerId::mouse : ui::PointerId::touchBase + e->pointerId());
        }

        ui::PointerKind KindOf(PointerEventData* e) { return e->pointerId() < 0 ? ui::PointerKind::Mouse : ui::PointerKind::Touch; }

        i32 ButtonOf(PointerEventData* e) { return i32(e->button()); }

        // The panels' components the event system made, by panel.
        struct Made
        {
            u64 panel;
            engine::PPtr<engine::GameObject> object;
        };

        Array<Made>& MadeList()
        {
            static Array<Made>* m = new Array<Made>();
            return *m;
        }
    }

    // ------------------------------------------------------------------------------------------------ PanelRaycaster

    ui::UIView* PanelRaycaster::panelViewInternal() const { return PanelInput::FirstView(panel_); }

    i32 PanelRaycaster::sortOrderPriority() const
    {
        // A screen panel by its sorting order; a world space panel by distance among the world's raycasts.
        if (!PanelInput::Alive(panel_) || PanelInput::IsWorld(panel_))
            return BaseRaycaster::sortOrderPriority();
        return i32(PanelInput::SortingOrder(panel_));
    }

    i32 PanelRaycaster::renderOrderPriority() const
    {
        // Over the canvases of the same order; the top-most panel first.
        i32 index = PanelInput::InputIndex(panel_);
        if (index < 0 || PanelInput::IsWorld(panel_))
            return BaseRaycaster::renderOrderPriority();
        return std::numeric_limits<i32>::max() - index;
    }

    void PanelRaycaster::RaycastInternal(PointerEventData* eventData)
    {
        if (!PanelInput::Alive(panel_))
            return;
        Vector2 at;
        f32 distance = 0;
        if (!PanelInput::ScreenToPanel(panel_, FromBottom(eventData->position()), at, distance) || !PanelInput::Pick(panel_, at))
            return;
        RaycastResult r;
        r.gameObject = engine::PPtr<engine::GameObject>(gameObject());
        r.module = engine::PPtr<engine::Object>(this);
        r.distance = distance;
        r.screenPosition = eventData->position();
        r.displayIndex = eventData->displayIndex();
        r.index = f32(Results().Size());
        r.sortingOrder = i32(PanelInput::SortingOrder(panel_));
        Results().Push(r);
    }

    // -------------------------------------------------------------------------------------------- PanelEventHandler

    ui::UIView* PanelEventHandler::panelViewInternal() const { return PanelInput::FirstView(panel_); }

    bool PanelEventHandler::PanelPoint(PointerEventData* eventData, Vector2& at) const
    {
        f32 distance = 0;
        return PanelInput::ScreenToPanel(panel_, FromBottom(eventData->position()), at, distance);
    }

    void PanelEventHandler::SendMove(PointerEventData* eventData, bool covered)
    {
        ui::EventSystem* events = PanelInput::Events(panel_);
        if (!events)
            return;
        Vector2 at(-1.0e9f, -1.0e9f);
        bool on = PanelPoint(eventData, at);
        lastScreen_ = eventData->position();
        events->PointerMove(UiPointerId(eventData), KindOf(eventData), at, 0, Now(), covered || !on);
    }

    void PanelEventHandler::OnPointerMove(PointerEventData* eventData) { SendMove(eventData, false); }

    void PanelEventHandler::OnPointerEnter(PointerEventData* eventData) { SendMove(eventData, false); }

    void PanelEventHandler::OnPointerExit(PointerEventData* eventData)
    {
        // Left the panel (another raycast is over it now): hover ends; a press goes on to its release.
        if (!pressed_)
            SendMove(eventData, true);
    }

    void PanelEventHandler::OnPointerDown(PointerEventData* eventData)
    {
        ui::EventSystem* events = PanelInput::Events(panel_);
        Vector2 at;
        if (!events || !PanelPoint(eventData, at))
            return;
        pressed_ = true;
        lastScreen_ = eventData->position();
        // The panel is selected: its keys come to it.
        if (EventSystem* s = EventSystem::current(); s && s->currentSelectedGameObject() != gameObject())
            s->SetSelectedGameObject(gameObject(), eventData);
        events->PointerDown(UiPointerId(eventData), KindOf(eventData), ButtonOf(eventData), at, 0, Now());
    }

    void PanelEventHandler::OnPointerUp(PointerEventData* eventData)
    {
        ui::EventSystem* events = PanelInput::Events(panel_);
        pressed_ = false;
        if (!events)
            return;
        Vector2 at(-1.0e9f, -1.0e9f);
        PanelPoint(eventData, at);
        events->PointerUp(UiPointerId(eventData), KindOf(eventData), ButtonOf(eventData), at, 0, Now());
    }

    // The panel makes its own clicks from the press and the release.
    void PanelEventHandler::OnPointerClick(PointerEventData* eventData) { eventData->Use(); }

    void PanelEventHandler::OnScroll(PointerEventData* eventData)
    {
        ui::EventSystem* events = PanelInput::Events(panel_);
        Vector2 at;
        if (!events || !PanelPoint(eventData, at))
            return;
        InputModule* m = eventData->currentInputModule();
        Vector2 ticks = m ? m->ConvertPointerEventScrollDeltaToTicks(eventData->scrollDelta()) : eventData->scrollDelta();
        events->Wheel(at, ticks, 0, Now());
        eventData->Use();
    }

    // Navigation and submits reach the panel as its keys (Update): the event system's are used up here.
    void PanelEventHandler::OnSubmit(BaseEventData* eventData) { eventData->Use(); }
    void PanelEventHandler::OnCancel(BaseEventData* eventData) { eventData->Use(); }
    void PanelEventHandler::OnMove(AxisEventData* eventData) { eventData->Use(); }

    void PanelEventHandler::OnSelect(BaseEventData* eventData)
    {
        (void)eventData;
        PanelInput::SetKeyboardPanel(panel_);
    }

    void PanelEventHandler::OnDeselect(BaseEventData* eventData)
    {
        (void)eventData;
        if (ui::EventSystem* events = PanelInput::Events(panel_))
            if (ui::ElementHandle f = events->Focused(); f.IsValid())
                events->Blur(f);
    }

    void PanelEventHandler::Update()
    {
        ui::EventSystem* events = PanelInput::Events(panel_);
        if (!events)
            return;
        // A press on the panel keeps its moves (the panel's capture) while the pointer is outside its raycast.
        if (pressed_)
        {
            EventSystem* s = EventSystem::current();
            BaseInput* in = s && s->currentInputModule() ? s->currentInputModule()->input() : nullptr;
            if (in)
            {
                Vector2 p = in->mousePosition();
                if (p.x != lastScreen_.x || p.y != lastScreen_.y)
                {
                    lastScreen_ = p;
                    Vector2 at(-1.0e9f, -1.0e9f);
                    f32 distance = 0;
                    PanelInput::ScreenToPanel(panel_, FromBottom(p), at, distance);
                    events->PointerMove(ui::PointerId::mouse, ui::PointerKind::Mouse, at, 0, Now());
                }
            }
        }
        // The frame's keys, text and composition while selected.
        EventSystem* s = EventSystem::current();
        if (!s || s->currentSelectedGameObject() != gameObject())
            return;
        u64 frame = engine::InputFeed::FrameNumber();
        if (frame == keysFrame_)
            return;
        keysFrame_ = frame;
        for (const engine::InputEvent& e : engine::InputFeed::Frame())
            switch (e.event.type)
            {
            case platform::EventType::KeyDown: events->KeyDown(u16(e.event.key.key), e.event.modifiers, e.time); break;
            case platform::EventType::KeyUp: events->KeyUp(u16(e.event.key.key), e.event.modifiers, e.time); break;
            case platform::EventType::Text: events->Character(e.event.text.codepoint, e.event.modifiers, e.time); break;
            case platform::EventType::Composition: events->SetComposition(engine::InputFeed::Composition(), e.event.composition.cursor, e.time); break;
            default: break;
            }
    }

    bool PanelEventHandler::Handles(EventKind kind) const
    {
        switch (kind)
        {
        case EventKind::PointerMove:
        case EventKind::PointerUp:
        case EventKind::PointerDown:
        case EventKind::Submit:
        case EventKind::Cancel:
        case EventKind::Move:
        case EventKind::Scroll:
        case EventKind::Select:
        case EventKind::Deselect:
        case EventKind::PointerExit:
        case EventKind::PointerEnter:
        case EventKind::PointerClick: return true;
        default: return false;
        }
    }

    void PanelEventHandler::HandleEvent(EventKind kind, BaseEventData* data)
    {
        auto* p = static_cast<PointerEventData*>(data);
        switch (kind)
        {
        case EventKind::PointerMove: OnPointerMove(p); break;
        case EventKind::PointerUp: OnPointerUp(p); break;
        case EventKind::PointerDown: OnPointerDown(p); break;
        case EventKind::Submit: OnSubmit(data); break;
        case EventKind::Cancel: OnCancel(data); break;
        case EventKind::Move: OnMove(static_cast<AxisEventData*>(data)); break;
        case EventKind::Scroll: OnScroll(p); break;
        case EventKind::Select: OnSelect(data); break;
        case EventKind::Deselect: OnDeselect(data); break;
        case EventKind::PointerExit: OnPointerExit(p); break;
        case EventKind::PointerEnter: OnPointerEnter(p); break;
        case EventKind::PointerClick: OnPointerClick(p); break;
        default: break;
        }
    }

    void PanelEventHandler::OnDisable()
    {
        pressed_ = false;
        UIBehaviour::OnDisable();
    }

    void PanelEventHandler::UpdatePanelComponents(EventSystem* system)
    {
        Array<Made>& made = MadeList();
        // The panels that are gone take their objects with them.
        for (usize i = 0; i < made.Size();)
        {
            if (!PanelInput::Alive(made[i].panel) || !made[i].object.Get())
            {
                if (engine::GameObject* g = made[i].object.Get())
                    engine::Object::DestroyImmediate(g);
                made.RemoveAt(i);
                continue;
            }
            i++;
        }
        ui::PanelInputConfiguration* config = ui::PanelInputConfiguration::current();
        bool create = !config || config->autoCreatePanelComponents();
        if (!system || !create || PanelInput::ReadsOwnInput())
        {
            RemovePanelComponents();
            return;
        }
        for (u64 panel : PanelInput::Panels())
        {
            bool have = false;
            for (const Made& m : made)
                have = have || m.panel == panel;
            if (have)
                continue;
            engine::GameObject* g = engine::GameObject::Create(PanelInput::Name(panel));
            g->transform()->SetParent(system->gameObject()->transform(), false);
            auto* handler = static_cast<PanelEventHandler*>(g->AddComponent(scripting::TypeOf<PanelEventHandler>()));
            auto* raycaster = static_cast<PanelRaycaster*>(g->AddComponent(scripting::TypeOf<PanelRaycaster>()));
            if (handler)
                handler->SetPanelId(panel);
            if (raycaster)
                raycaster->SetPanelId(panel);
            made.Push(Made{panel, engine::PPtr<engine::GameObject>(g)});
        }
    }

    void PanelEventHandler::RemovePanelComponents()
    {
        Array<Made>& made = MadeList();
        for (const Made& m : made)
            if (engine::GameObject* g = m.object.Get())
                engine::Object::DestroyImmediate(g);
        made.Clear();
    }
}
