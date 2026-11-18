#include <Helix/CanvasUI/Events/EventSystem.h>
#include <Helix/CanvasUI/Events/PanelEvents.h>
#include <Helix/UI/Engine/PanelInput.h>

#include <Helix/CanvasUI/Core/Implementation/CanvasUILog.h>
#include <Helix/CanvasUI/Core/ManagedCalls.h>
#include <Helix/Engine/Script/ScriptClass.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Engine/Transform/Transform.h>

#include <cmath>

namespace helix::canvasui
{
    // ------------------------------------------------------------------------------------------------ the input

    namespace
    {
        constexpr f32 kDoubleClickTime = 0.3f;

        void AppendUtf8(String& s, u32 c)
        {
            if (c < 0x80)
                s += char(c);
            else if (c < 0x800)
                s += char(0xC0 | (c >> 6)), s += char(0x80 | (c & 0x3F));
            else if (c < 0x10000)
                s += char(0xE0 | (c >> 12)), s += char(0x80 | ((c >> 6) & 0x3F)), s += char(0x80 | (c & 0x3F));
            else
                s += char(0xF0 | (c >> 18)), s += char(0x80 | ((c >> 12) & 0x3F)), s += char(0x80 | ((c >> 6) & 0x3F)), s += char(0x80 | (c & 0x3F));
        }

        // The frame's input as BaseInput reads it, brought up to date once a frame from the input feed: the mouse (its
        // position from the bottom left, its buttons down, up and held), the wheel, the touches with their phases, the
        // keys of the navigation, the input method's text.
        struct InputState
        {
            u64 frame = ~u64(0), resets = 0;
            Vector2 mouse = Vector2(0, 0);
            bool mouseSeen = false;
            bool held[3] = {}, down[3] = {}, up[3] = {};
            Vector2 wheel = Vector2(0, 0);
            struct Finger
            {
                engine::Touch touch;
                bool live = false;
            };
            Array<Finger> fingers;
            // Keys held and pressed this frame, by key code.
            Array<u32> keysHeld, keysDown;
            String composition;
            engine::IMECompositionMode imeMode = engine::IMECompositionMode::Auto;
            Vector2 compositionCursor = Vector2(0, 0);

            static bool Has(const Array<u32>& keys, u32 k)
            {
                for (u32 x : keys)
                    if (x == k)
                        return true;
                return false;
            }

            Vector2 FromTop(f32 x, f32 y) const { return Vector2(x, f32(engine::Screen::height()) - y); }

            void Update()
            {
                u64 now = engine::InputFeed::FrameNumber();
                if (now == frame)
                    return;
                frame = now;
                // A reset of the feed (focus lost, a host's restart) lets go of what was held.
                if (engine::InputFeed::ResetNumber() != resets)
                {
                    resets = engine::InputFeed::ResetNumber();
                    for (int b = 0; b < 3; b++)
                        held[b] = false;
                    keysHeld.Clear();
                    fingers.Clear();
                }
                for (int b = 0; b < 3; b++)
                    down[b] = up[b] = false;
                wheel = Vector2(0, 0);
                keysDown.Clear();
                // Touches that ended are gone; the others are stationary until they move.
                for (usize i = 0; i < fingers.Size();)
                {
                    engine::TouchPhase p = fingers[i].touch.phase;
                    if (p == engine::TouchPhase::Ended || p == engine::TouchPhase::Canceled)
                    {
                        fingers.RemoveAt(i);
                        continue;
                    }
                    fingers[i].touch.phase = engine::TouchPhase::Stationary;
                    fingers[i].touch.deltaPosition = Vector2(0, 0);
                    i++;
                }
                for (const engine::InputEvent& e : engine::InputFeed::Frame())
                {
                    const platform::Event& ev = e.event;
                    switch (ev.type)
                    {
                    case platform::EventType::MouseMove:
                        mouse = FromTop(ev.mouse.x, ev.mouse.y), mouseSeen = true;
                        break;
                    case platform::EventType::MouseDown:
                    case platform::EventType::MouseUp:
                    {
                        mouse = FromTop(ev.mouse.x, ev.mouse.y), mouseSeen = true;
                        i32 b = ev.mouse.button == platform::MouseButton::Left ? 0 : ev.mouse.button == platform::MouseButton::Right ? 1
                                                                                  : ev.mouse.button == platform::MouseButton::Middle ? 2 : -1;
                        if (b < 0)
                            break;
                        bool pressed = ev.type == platform::EventType::MouseDown;
                        (pressed ? down : up)[b] = true;
                        held[b] = pressed;
                        break;
                    }
                    case platform::EventType::MouseWheel: wheel = Vector2(wheel.x + ev.wheel.dx, wheel.y + ev.wheel.dy); break;
                    case platform::EventType::KeyDown:
                        if (!ev.repeat && !Has(keysHeld, u32(ev.key.key)))
                        {
                            keysHeld.Push(u32(ev.key.key));
                            keysDown.Push(u32(ev.key.key));
                        }
                        break;
                    case platform::EventType::KeyUp:
                        for (usize i = 0; i < keysHeld.Size(); i++)
                            if (keysHeld[i] == u32(ev.key.key))
                            {
                                keysHeld.RemoveAt(i);
                                break;
                            }
                        break;
                    case platform::EventType::TouchBegin:
                    case platform::EventType::TouchMove:
                    case platform::EventType::TouchEnd:
                    case platform::EventType::TouchCancel:
                    {
                        Finger* f = nullptr;
                        for (Finger& x : fingers)
                            if (x.touch.fingerId == ev.touch.id)
                                f = &x;
                        Vector2 at = FromTop(ev.touch.x, ev.touch.y);
                        if (!f)
                        {
                            fingers.Push(Finger());
                            f = &fingers[fingers.Size() - 1];
                            f->touch.fingerId = ev.touch.id;
                            f->touch.position = f->touch.rawPosition = at;
                            f->touch.tapCount = 1;
                        }
                        f->touch.deltaPosition = Vector2(at.x - f->touch.position.x, at.y - f->touch.position.y);
                        f->touch.position = at;
                        f->touch.phase = ev.type == platform::EventType::TouchBegin  ? engine::TouchPhase::Began
                                         : ev.type == platform::EventType::TouchMove ? engine::TouchPhase::Moved
                                         : ev.type == platform::EventType::TouchEnd  ? engine::TouchPhase::Ended
                                                                                     : engine::TouchPhase::Canceled;
                        break;
                    }
                    default: break;
                    }
                }
                composition.Clear();
                for (u32 c : engine::InputFeed::Composition())
                    AppendUtf8(composition, c);
            }

            // The default input settings' axes and buttons: Horizontal (arrows, A and D), Vertical (arrows, W and S),
            // Submit (Return, keypad Enter, Space), Cancel (Escape).
            f32 Axis(StringView name)
            {
                using K = platform::KeyCode;
                auto held2 = [&](K a, K b) { return Has(keysHeld, u32(a)) || Has(keysHeld, u32(b)); };
                if (name == StringView("Horizontal"))
                    return (held2(K::RightArrow, K::D) ? 1.0f : 0.0f) - (held2(K::LeftArrow, K::A) ? 1.0f : 0.0f);
                if (name == StringView("Vertical"))
                    return (held2(K::UpArrow, K::W) ? 1.0f : 0.0f) - (held2(K::DownArrow, K::S) ? 1.0f : 0.0f);
                return 0;
            }

            bool ButtonDown(StringView name)
            {
                using K = platform::KeyCode;
                auto any = [&](std::initializer_list<K> keys) {
                    for (K k : keys)
                        if (Has(keysDown, u32(k)))
                            return true;
                    return false;
                };
                if (name == StringView("Submit"))
                    return any({K::Return, K::KeypadEnter, K::Space});
                if (name == StringView("Cancel"))
                    return any({K::Escape});
                if (name == StringView("Horizontal"))
                    return any({K::RightArrow, K::D, K::LeftArrow, K::A});
                if (name == StringView("Vertical"))
                    return any({K::UpArrow, K::W, K::DownArrow, K::S});
                return false;
            }
        };

        InputState& In()
        {
            static InputState* s = new InputState();
            s->Update();
            return *s;
        }
    }

    String BaseInput::compositionString() const { return In().composition; }
    engine::IMECompositionMode BaseInput::imeCompositionMode() const { return In().imeMode; }
    void BaseInput::SetImeCompositionMode(engine::IMECompositionMode value) { In().imeMode = value; }
    Vector2 BaseInput::compositionCursorPos() const { return In().compositionCursor; }

    void BaseInput::SetCompositionCursorPos(Vector2 value)
    {
        In().compositionCursor = value;
        // The input method's caret where the text field asks (the host turns it on for a frame that asks).
        engine::InputFeed::RequestTextInput(i32(value.x), i32(f32(engine::Screen::height()) - value.y), 16);
    }

    bool BaseInput::mousePresent() const { return true; }
    Vector2 BaseInput::mousePosition() const { return In().mouse; }
    Vector2 BaseInput::mouseScrollDelta() const { return In().wheel; }
    bool BaseInput::touchSupported() const
    {
#if HX_PLATFORM_ANDROID || HX_PLATFORM_WEB
        return true;
#else
        return false;
#endif
    }
    i32 BaseInput::touchCount() const { return i32(In().fingers.Size()); }

    engine::Touch BaseInput::GetTouch(i32 index)
    {
        InputState& s = In();
        return index >= 0 && usize(index) < s.fingers.Size() ? s.fingers[usize(index)].touch : engine::Touch();
    }

    bool BaseInput::GetMouseButtonDown(i32 button) { return button >= 0 && button < 3 && In().down[button]; }
    bool BaseInput::GetMouseButtonUp(i32 button) { return button >= 0 && button < 3 && In().up[button]; }
    bool BaseInput::GetMouseButton(i32 button) { return button >= 0 && button < 3 && In().held[button]; }
    f32 BaseInput::GetAxisRaw(StringView axisName) { return In().Axis(axisName); }
    bool BaseInput::GetButtonDown(StringView buttonName) { return In().ButtonDown(buttonName); }

    // ------------------------------------------------------------------------------------------------ raycasters

    namespace
    {
        Array<engine::PPtr<engine::Object>>& Raycasters()
        {
            static Array<engine::PPtr<engine::Object>>* r = new Array<engine::PPtr<engine::Object>>();
            return *r;
        }

        Array<engine::PPtr<engine::Object>>& Systems()
        {
            static Array<engine::PPtr<engine::Object>>* s = new Array<engine::PPtr<engine::Object>>();
            return *s;
        }
    }

    void BaseRaycaster::RaycastInternal(PointerEventData* eventData) { managedRaycast().Invoke(this, eventData); }

    Array<RaycastResultData> BaseRaycaster::RaycastNative(PointerEventData* eventData)
    {
        Array<RaycastResult>& sink = Results();
        Array<RaycastResult> saved = sink;
        sink.Clear();
        // The C++ raycast of this class (not a C# override's).
        if (!ManagedCalls::IsManaged(this) || script()->nativeBase != ~0u)
            RaycastInternal(eventData);
        Array<RaycastResultData> out;
        for (const RaycastResult& r : sink)
            out.Push(RaycastResultData::From(r));
        sink = saved;
        return out;
    }

    void BaseRaycaster::AppendResultInternal(RaycastResultData result) { Results().Push(result.ToResult()); }

    engine::Event<BaseRaycaster*, PointerEventData*>& BaseRaycaster::managedRaycast()
    {
        static engine::Event<BaseRaycaster*, PointerEventData*>* e = new engine::Event<BaseRaycaster*, PointerEventData*>();
        return *e;
    }

    Array<RaycastResult>& BaseRaycaster::Results()
    {
        static Array<RaycastResult>* r = new Array<RaycastResult>();
        return *r;
    }

    void RaycasterManager::Add(BaseRaycaster* raycaster)
    {
        for (const engine::PPtr<engine::Object>& r : Raycasters())
            if (r.Get() == raycaster)
                return;
        Raycasters().Push(engine::PPtr<engine::Object>(raycaster));
    }

    void RaycasterManager::Remove(BaseRaycaster* raycaster)
    {
        Array<engine::PPtr<engine::Object>>& list = Raycasters();
        for (usize i = 0; i < list.Size(); i++)
            if (list[i].Get() == raycaster)
            {
                list.RemoveAt(i);
                return;
            }
    }

    Array<BaseRaycaster*> RaycasterManager::GetRaycastersInternal()
    {
        Array<BaseRaycaster*> out;
        Array<engine::PPtr<engine::Object>>& list = Raycasters();
        for (usize i = 0; i < list.Size();)
        {
            if (!list[i].Get())
            {
                list.RemoveAt(i);
                continue;
            }
            out.Push(static_cast<BaseRaycaster*>(list[i].Get()));
            i++;
        }
        return out;
    }

    void BaseRaycaster::OnEnable()
    {
        UIBehaviour::OnEnable();
        RaycasterManager::Add(this);
    }

    void BaseRaycaster::OnDisable()
    {
        RaycasterManager::Remove(this);
        UIBehaviour::OnDisable();
    }

    BaseRaycaster* BaseRaycaster::rootRaycaster()
    {
        if (!rootRaycaster_.Get())
        {
            BaseRaycaster* top = nullptr;
            for (engine::Transform* t = engine::Component::transform(); t; t = t->parent())
                if (engine::GameObject* g = t->gameObject(); g && g->activeInHierarchy())
                    if (BaseRaycaster* r = ComponentOf<BaseRaycaster>(g))
                        top = r;
            rootRaycaster_ = engine::PPtr<engine::Object>(top);
        }
        return static_cast<BaseRaycaster*>(rootRaycaster_.Get());
    }

    String BaseRaycaster::Describe() const
    {
        engine::Camera* c = eventCamera();
        return Format("Name: {}\neventCamera: {}\nsortOrderPriority: {}\nrenderOrderPriority: {}", gameObject() ? gameObject()->Name() : StringView("null"),
                      c ? c->Name() : StringView("null"), sortOrderPriority(), renderOrderPriority());
    }

    // ------------------------------------------------------------------------------------------------ EventSystem

    EventSystem::~EventSystem() { delete dummyData_; }

    EventSystem* EventSystem::current()
    {
        Array<engine::PPtr<engine::Object>>& s = Systems();
        for (usize i = 0; i < s.Size();)
        {
            if (!s[i].Get())
            {
                s.RemoveAt(i);
                continue;
            }
            return static_cast<EventSystem*>(s[i].Get());
        }
        return nullptr;
    }

    void EventSystem::SetCurrent(EventSystem* value)
    {
        Array<engine::PPtr<engine::Object>>& s = Systems();
        for (usize i = 0; i < s.Size(); i++)
            if (s[i].Get() == value)
            {
                if (i > 0)
                {
                    engine::PPtr<engine::Object> keep = s[i];
                    s.RemoveAt(i);
                    s.Insert(0, keep);
                }
                return;
            }
        if (value)
            HX_LOG_ERROR(CanvasUILog, "Failed setting EventSystem.current to unknown EventSystem {}", value->Name());
    }

    InputModule* EventSystem::currentInputModule() const { return static_cast<InputModule*>(currentModule_.Get()); }

    void EventSystem::UpdateModules()
    {
        modules_.Clear();
        engine::GameObject* g = gameObject();
        for (u32 i = 0; g && i < g->GetComponentCount(); i++)
        {
            engine::Component* c = g->GetComponentAtIndex(i);
            UIBehaviour* b = AsUIBehaviour(c);
            if (b && scripting::IsAssignableFrom(scripting::TypeOf<InputModule>(), b->script()->type) && b->isActiveAndEnabled())
                modules_.Push(engine::PPtr<engine::Object>(b));
        }
    }

    BaseEventData* EventSystem::DummyData()
    {
        if (!dummyData_)
            dummyData_ = new BaseEventData(this);
        return dummyData_;
    }

    void EventSystem::SetSelectedGameObject(engine::GameObject* selected) { SetSelectedGameObject(selected, DummyData()); }

    void EventSystem::SetSelectedGameObject(engine::GameObject* selected, BaseEventData* pointer)
    {
        if (selectionGuard_)
        {
            HX_LOG_ERROR(CanvasUILog, "Attempting to select {} while already selecting an object.", selected ? selected->Name() : StringView("null"));
            return;
        }
        selectionGuard_ = true;
        if (selected == currentSelected_.Get())
        {
            selectionGuard_ = false;
            return;
        }
        ExecuteEvents::ExecuteInternal(currentSelected_.Get(), pointer, EventKind::Deselect);
        currentSelected_ = engine::PPtr<engine::GameObject>(selected);
        ExecuteEvents::ExecuteInternal(selected, pointer, EventKind::Select);
        selectionGuard_ = false;
    }

    namespace
    {
        // The raycasts in the order the events go: higher camera depth, sort order and render order first, then the
        // sorting layer and order, the depth within one root canvas, the distance, the order found.
        bool Before(const RaycastResult& l, const RaycastResult& r)
        {
            auto* lm = static_cast<BaseRaycaster*>(l.module.Get());
            auto* rm = static_cast<BaseRaycaster*>(r.module.Get());
            if (lm != rm && lm && rm)
            {
                engine::Camera* lc = lm->eventCamera();
                engine::Camera* rc = rm->eventCamera();
                if (lc && rc && lc->depth() != rc->depth())
                    return lc->depth() > rc->depth();
                if (lm->sortOrderPriority() != rm->sortOrderPriority())
                    return lm->sortOrderPriority() > rm->sortOrderPriority();
                if (lm->renderOrderPriority() != rm->renderOrderPriority())
                    return lm->renderOrderPriority() > rm->renderOrderPriority();
            }
            if (l.sortingLayer != r.sortingLayer)
                return l.sortingLayer > r.sortingLayer;
            if (l.sortingOrder != r.sortingOrder)
                return l.sortingOrder > r.sortingOrder;
            if (l.depth != r.depth && lm && rm && lm->rootRaycaster() == rm->rootRaycaster())
                return l.depth > r.depth;
            if (l.distance != r.distance)
                return l.distance < r.distance;
            return l.index < r.index;
        }
    }

    void EventSystem::RaycastAll(PointerEventData* eventData, Array<RaycastResult>& results)
    {
        results.Clear();
        Array<RaycastResult>& sink = BaseRaycaster::Results();
        for (BaseRaycaster* r : RaycasterManager::GetRaycastersInternal())
        {
            if (!r->IsActive())
                continue;
            sink.Clear();
            for (const RaycastResult& x : results)
                sink.Push(x);
            // A raycaster of a C# class raycasts through its C# override (which may call the native one as its base).
            if (ManagedCalls::IsManaged(r))
                BaseRaycaster::managedRaycast().Invoke(r, eventData);
            else
                r->RaycastInternal(eventData);
            results.Clear();
            for (const RaycastResult& x : sink)
                results.Push(x);
        }
        sink.Clear();
        // A stable insertion sort (the order found decides among equals).
        for (usize i = 1; i < results.Size(); i++)
            for (usize j = i; j > 0 && Before(results[j], results[j - 1]); j--)
                std::swap(results[j - 1], results[j]);
    }

    Array<RaycastResultData> EventSystem::RaycastAllInternal(PointerEventData* eventData)
    {
        Array<RaycastResult> results;
        RaycastAll(eventData, results);
        Array<RaycastResultData> out;
        for (const RaycastResult& r : results)
            out.Push(RaycastResultData::From(r));
        return out;
    }

    bool EventSystem::IsPointerOverGameObject() { return IsPointerOverGameObject(PointerInputModule::kMouseLeftId); }

    bool EventSystem::IsPointerOverGameObject(i32 pointerId)
    {
        InputModule* m = currentInputModule();
        return m && m->IsPointerOverGameObject(pointerId);
    }

    String EventSystem::Describe() const
    {
        engine::GameObject* s = currentSelected_.Get();
        InputModule* m = currentInputModule();
        return Format("<b>Selected:</b>{}\n\n{}", s ? s->Name() : StringView(""), m ? m->Name() : StringView("No module"));
    }

    void EventSystem::OnEnable()
    {
        UIBehaviour::OnEnable();
        Systems().Push(engine::PPtr<engine::Object>(this));
        // The panels' input comes through an event system now (AutoRedirect).
        ui::PanelInput::SetEventSystemPresent(true);
    }

    void EventSystem::OnDisable()
    {
        if (InputModule* m = currentInputModule())
        {
            m->DeactivateModule();
            currentModule_ = engine::PPtr<engine::Object>();
        }
        Array<engine::PPtr<engine::Object>>& s = Systems();
        for (usize i = 0; i < s.Size(); i++)
            if (s[i].Get() == this)
            {
                s.RemoveAt(i);
                break;
            }
        // The last one gone: the panels read their own input again.
        if (s.Empty())
        {
            ui::PanelInput::SetEventSystemPresent(false);
            PanelEventHandler::RemovePanelComponents();
        }
        UIBehaviour::OnDisable();
    }

    void EventSystem::Start() { UIBehaviour::Start(); }

    void EventSystem::TickModules()
    {
        for (const engine::PPtr<engine::Object>& m : modules_)
            if (auto* module = static_cast<InputModule*>(m.Get()))
                module->UpdateModule();
    }

    void EventSystem::ChangeEventModule(InputModule* module)
    {
        if (currentInputModule() == module)
            return;
        if (InputModule* old = currentInputModule())
            old->DeactivateModule();
        if (module)
            module->ActivateModule();
        currentModule_ = engine::PPtr<engine::Object>(module);
    }

    void EventSystem::Update()
    {
        if (current() != this)
            return;
        PanelEventHandler::UpdatePanelComponents(this);
        TickModules();
        bool changed = false;
        for (const engine::PPtr<engine::Object>& m : modules_)
        {
            auto* module = static_cast<InputModule*>(m.Get());
            if (module && module->IsModuleSupported() && module->ShouldActivateModule())
            {
                if (currentInputModule() != module)
                {
                    ChangeEventModule(module);
                    changed = true;
                }
                break;
            }
        }
        // None wants to: the first supported one.
        if (!currentInputModule())
            for (const engine::PPtr<engine::Object>& m : modules_)
                if (auto* module = static_cast<InputModule*>(m.Get()); module && module->IsModuleSupported())
                {
                    ChangeEventModule(module);
                    changed = true;
                    break;
                }
        if (!changed)
            if (InputModule* module = currentInputModule())
                module->Process();
    }

    // -------------------------------------------------------------------------------------------- InputModule

    InputModule::~InputModule()
    {
        delete axisEventData_;
        delete baseEventData_;
    }

    EventSystem* InputModule::eventSystem() const { return ComponentOf<EventSystem>(gameObject()); }

    BaseInput* InputModule::input()
    {
        if (BaseInput* o = inputOverride())
            return o;
        if (!defaultInput_.Get())
        {
            engine::GameObject* g = gameObject();
            for (u32 i = 0; g && i < g->GetComponentCount() && !defaultInput_.Get(); i++)
            {
                UIBehaviour* b = AsUIBehaviour(g->GetComponentAtIndex(i));
                // A BaseInput of that very class (not one deriving from it).
                if (b && b->script()->type == scripting::TypeOf<BaseInput>())
                    defaultInput_ = engine::PPtr<engine::Object>(b);
            }
            if (!defaultInput_.Get() && g)
                defaultInput_ = engine::PPtr<engine::Object>(g->AddComponent(scripting::TypeOf<BaseInput>()));
        }
        return static_cast<BaseInput*>(defaultInput_.Get());
    }

    void InputModule::OnEnable()
    {
        UIBehaviour::OnEnable();
        if (EventSystem* s = eventSystem())
            s->UpdateModules();
    }

    void InputModule::OnDisable()
    {
        if (EventSystem* s = eventSystem())
            s->UpdateModules();
        UIBehaviour::OnDisable();
    }

    i32 InputModule::ConvertUIToolkitPointerId(PointerEventData* sourcePointerData)
    {
        // The UI's pointer ids: the mouse 0, touches from 1.
        return !sourcePointerData || sourcePointerData->pointerId() < 0 ? 0 : 1 + sourcePointerData->pointerId();
    }

    Vector2 InputModule::ConvertPointerEventScrollDeltaToTicks(Vector2 scrollDelta)
    {
        f32 perTick = input()->mouseScrollDeltaPerTick();
        if (std::fabs(perTick) < 1e-38f)
            return Vector2(0, 0);
        return Vector2(scrollDelta.x / perTick, scrollDelta.y / perTick);
    }

    RaycastResult InputModule::FindFirstRaycast(Span<const RaycastResult> candidates)
    {
        for (const RaycastResult& r : candidates)
            if (r.gameObject.Get())
                return r;
        return RaycastResult();
    }

    RaycastResultData InputModule::FindFirstRaycastData(Span<const RaycastResultData> candidates)
    {
        for (const RaycastResultData& r : candidates)
            if (engine::EntityIdToObject(r.gameObject))
                return r;
        return RaycastResultData();
    }

    MoveDirection InputModule::DetermineMoveDirection(f32 x, f32 y) { return DetermineMoveDirection(x, y, 0.6f); }

    MoveDirection InputModule::DetermineMoveDirection(f32 x, f32 y, f32 deadZone)
    {
        if (x * x + y * y < deadZone * deadZone)
            return MoveDirection::None;
        if (std::fabs(x) > std::fabs(y))
            return x > 0 ? MoveDirection::Right : MoveDirection::Left;
        return y > 0 ? MoveDirection::Up : MoveDirection::Down;
    }

    engine::GameObject* InputModule::FindCommonRoot(engine::GameObject* g1, engine::GameObject* g2)
    {
        if (!g1 || !g2)
            return nullptr;
        for (engine::Transform* t1 = g1->transform(); t1; t1 = t1->parent())
            for (engine::Transform* t2 = g2->transform(); t2; t2 = t2->parent())
                if (t1 == t2)
                    return t1->gameObject();
        return nullptr;
    }

    void InputModule::HandlePointerExitAndEnter(PointerEventData* data, engine::GameObject* newEnterTarget)
    {
        auto& hovered = data->hovered();
        // Nothing entered now (or the entered object is gone): exits to everything tracked.
        if (!newEnterTarget || !data->pointerEnter())
        {
            Array<engine::PPtr<engine::GameObject>> left = hovered;
            for (const engine::PPtr<engine::GameObject>& h : left)
            {
                data->SetFullyExited(true);
                ExecuteEvents::ExecuteInternal(h.Get(), data, EventKind::PointerExit);
            }
            hovered.Clear();
            if (!newEnterTarget)
            {
                data->SetPointerEnter(nullptr);
                return;
            }
        }
        // The same target: moves only.
        if (data->pointerEnter() == newEnterTarget)
        {
            if (data->IsPointerMoving())
            {
                Array<engine::PPtr<engine::GameObject>> now = hovered;
                for (const engine::PPtr<engine::GameObject>& h : now)
                    ExecuteEvents::ExecuteInternal(h.Get(), data, EventKind::PointerMove);
            }
            return;
        }
        engine::GameObject* commonRoot = FindCommonRoot(data->pointerEnter(), newEnterTarget);
        engine::GameObject* exitParent = ExecuteEvents::GetEventHandlerInternal(newEnterTarget, EventKind::PointerExit);
        auto removeHovered = [&](engine::GameObject* g) {
            for (usize i = 0; i < hovered.Size(); i++)
                if (hovered[i].Get() == g)
                {
                    hovered.RemoveAt(i);
                    return;
                }
        };
        if (engine::GameObject* old = data->pointerEnter())
        {
            for (engine::Transform* t = old->transform(); t;)
            {
                if (sendPointerHoverToParent_ && commonRoot && commonRoot->transform() == t)
                    break;
                if (!sendPointerHoverToParent_ && exitParent == t->gameObject())
                    break;
                data->SetFullyExited(t->gameObject() != commonRoot && data->pointerEnter() != newEnterTarget);
                ExecuteEvents::ExecuteInternal(t->gameObject(), data, EventKind::PointerExit);
                removeHovered(t->gameObject());
                if (sendPointerHoverToParent_)
                    t = t->parent();
                if (commonRoot && t && commonRoot->transform() == t)
                    break;
                if (!sendPointerHoverToParent_)
                    t = t->parent();
            }
        }
        engine::GameObject* oldEnter = data->pointerEnter();
        data->SetPointerEnter(newEnterTarget);
        for (engine::Transform* t = newEnterTarget->transform(); t;)
        {
            data->SetReentered(t->gameObject() == commonRoot && t->gameObject() != oldEnter);
            if (sendPointerHoverToParent_ && data->reentered())
                break;
            ExecuteEvents::ExecuteInternal(t->gameObject(), data, EventKind::PointerEnter);
            hovered.Push(engine::PPtr<engine::GameObject>(t->gameObject()));
            if (!sendPointerHoverToParent_ && ExecuteEvents::CanHandleEventInternal(t->gameObject(), EventKind::PointerEnter))
                break;
            if (sendPointerHoverToParent_)
                t = t->parent();
            if (commonRoot && t && commonRoot->transform() == t)
                break;
            if (!sendPointerHoverToParent_)
                t = t->parent();
        }
    }

    AxisEventData* InputModule::GetAxisEventData(f32 x, f32 y, f32 moveDeadZone)
    {
        if (!axisEventData_)
            axisEventData_ = new AxisEventData(eventSystem());
        axisEventData_->Reset();
        axisEventData_->SetMoveVector(Vector2(x, y));
        axisEventData_->SetMoveDir(DetermineMoveDirection(x, y, moveDeadZone));
        return axisEventData_;
    }

    BaseEventData* InputModule::GetBaseEventData()
    {
        if (!baseEventData_)
            baseEventData_ = new BaseEventData(eventSystem());
        baseEventData_->Reset();
        return baseEventData_;
    }

    // ----------------------------------------------------------------------------------------- PointerInputModule

    PointerInputModule::~PointerInputModule()
    {
        for (Pointer& p : pointerData_)
            if (p.data->Wrapper() == 0)
                delete p.data;
    }

    bool PointerInputModule::PointerDataOf(i32 id, PointerEventData*& data, bool create)
    {
        for (Pointer& p : pointerData_)
            if (p.id == id)
            {
                data = p.data;
                return false;
            }
        data = nullptr;
        if (!create)
            return false;
        data = new PointerEventData(eventSystem());
        data->SetPointerId(id);
        pointerData_.Push({id, data});
        return true;
    }

    bool PointerInputModule::GetPointerData(i32 id, PointerEventData*& data, bool create) { return PointerDataOf(id, data, create); }

    void PointerInputModule::RemovePointerData(PointerEventData* data)
    {
        for (usize i = 0; i < pointerData_.Size(); i++)
            if (pointerData_[i].data == data)
            {
                if (data->Wrapper() == 0)
                    delete data;
                pointerData_.RemoveAt(i);
                return;
            }
    }

    PointerEventData* PointerInputModule::GetTouchPointerEventData(engine::Touch input, bool& pressed, bool& released)
    {
        PointerEventData* data = nullptr;
        bool created = PointerDataOf(input.fingerId, data, true);
        data->Reset();
        pressed = created || input.phase == engine::TouchPhase::Began;
        released = input.phase == engine::TouchPhase::Canceled || input.phase == engine::TouchPhase::Ended;
        if (created)
            data->SetPosition(input.position);
        data->SetDelta(pressed ? Vector2(0, 0) : Vector2(input.position.x - data->position().x, input.position.y - data->position().y));
        data->SetPosition(input.position);
        data->SetButton(PointerEventData::InputButton::Left);
        if (input.phase == engine::TouchPhase::Canceled)
            data->pointerCurrentRaycast = RaycastResult();
        else
        {
            eventSystem()->RaycastAll(data, raycastResultCache_);
            data->pointerCurrentRaycast = FindFirstRaycast(Span<const RaycastResult>(raycastResultCache_.Data(), raycastResultCache_.Size()));
            raycastResultCache_.Clear();
        }
        data->SetPressure(input.pressure);
        data->SetAltitudeAngle(input.altitudeAngle);
        data->SetAzimuthAngle(input.azimuthAngle);
        data->SetRadius(Vector2(input.radius, input.radius));
        data->SetRadiusVariance(Vector2(input.radiusVariance, input.radiusVariance));
        return data;
    }

    void PointerInputModule::CopyFromTo(PointerEventData* from, PointerEventData* to)
    {
        to->SetPosition(from->position());
        to->SetDelta(from->delta());
        to->SetScrollDelta(from->scrollDelta());
        to->pointerCurrentRaycast = from->pointerCurrentRaycast;
        to->SetPointerEnter(from->pointerEnter());
        to->SetPressure(from->pressure());
        to->SetTangentialPressure(from->tangentialPressure());
        to->SetAltitudeAngle(from->altitudeAngle());
        to->SetAzimuthAngle(from->azimuthAngle());
        to->SetTwist(from->twist());
        to->SetRadius(from->radius());
        to->SetRadiusVariance(from->radiusVariance());
    }

    PointerEventData::FramePressState PointerInputModule::StateForMouseButton(i32 buttonId)
    {
        BaseInput* in = input();
        bool pressed = in->GetMouseButtonDown(buttonId), released = in->GetMouseButtonUp(buttonId);
        if (pressed && released)
            return PointerEventData::FramePressState::PressedAndReleased;
        if (pressed)
            return PointerEventData::FramePressState::Pressed;
        if (released)
            return PointerEventData::FramePressState::Released;
        return PointerEventData::FramePressState::NotChanged;
    }

    void PointerInputModule::MouseFrame(i32 id, ButtonFrame (&buttons)[3])
    {
        (void)id;
        BaseInput* in = input();
        PointerEventData* left = nullptr;
        bool created = PointerDataOf(kMouseLeftId, left, true);
        left->Reset();
        Vector2 pos = in->mousePosition();
        if (created)
            left->SetPosition(pos);
        left->SetDelta(Vector2(pos.x - left->position().x, pos.y - left->position().y));
        left->SetPosition(pos);
        left->SetScrollDelta(in->mouseScrollDelta());
        left->SetButton(PointerEventData::InputButton::Left);
        eventSystem()->RaycastAll(left, raycastResultCache_);
        left->pointerCurrentRaycast = FindFirstRaycast(Span<const RaycastResult>(raycastResultCache_.Data(), raycastResultCache_.Size()));
        raycastResultCache_.Clear();
        PointerEventData *right = nullptr, *middle = nullptr;
        PointerDataOf(kMouseRightId, right, true);
        right->Reset();
        CopyFromTo(left, right);
        right->SetButton(PointerEventData::InputButton::Right);
        PointerDataOf(kMouseMiddleId, middle, true);
        middle->Reset();
        CopyFromTo(left, middle);
        middle->SetButton(PointerEventData::InputButton::Middle);
        buttons[0] = {left, StateForMouseButton(0)};
        buttons[1] = {right, StateForMouseButton(1)};
        buttons[2] = {middle, StateForMouseButton(2)};
    }

    void PointerInputModule::MouseButtonInternal(i32 id, i32 button, PointerEventData*& data, i32& state)
    {
        // The left button's pass raycasts and copies (as the reference's GetMousePointerEventData); the others read it.
        static ButtonFrame frame[3];
        if (button == 0)
            MouseFrame(id, frame);
        data = button >= 0 && button < 3 ? frame[button].data : nullptr;
        state = button >= 0 && button < 3 ? i32(frame[button].state) : i32(PointerEventData::FramePressState::NotChanged);
    }

    PointerEventData* PointerInputModule::GetLastPointerEventData(i32 id)
    {
        PointerEventData* data = nullptr;
        PointerDataOf(id, data, false);
        return data;
    }

    void PointerInputModule::ProcessMove(PointerEventData* pointerEvent)
    {
        HandlePointerExitAndEnter(pointerEvent, pointerEvent->pointerCurrentRaycast.gameObject.Get());
    }

    void PointerInputModule::ProcessDrag(PointerEventData* e)
    {
        if (!e->IsPointerMoving() || !e->pointerDrag())
            return;
        if (!e->dragging())
        {
            Vector2 d(e->pressPosition().x - e->position().x, e->pressPosition().y - e->position().y);
            f32 threshold = f32(eventSystem()->pixelDragThreshold());
            if (!e->useDragThreshold() || d.x * d.x + d.y * d.y >= threshold * threshold)
            {
                ExecuteEvents::ExecuteInternal(e->pointerDrag(), e, EventKind::BeginDrag);
                e->SetDragging(true);
            }
        }
        if (e->dragging())
        {
            // The press ends where a drag begins.
            if (e->pointerPress() != e->pointerDrag())
            {
                ExecuteEvents::ExecuteInternal(e->pointerPress(), e, EventKind::PointerUp);
                e->SetEligibleForClick(false);
                e->SetPointerPress(nullptr);
                e->SetRawPointerPress(nullptr);
            }
            ExecuteEvents::ExecuteInternal(e->pointerDrag(), e, EventKind::Drag);
        }
    }

    bool PointerInputModule::IsPointerOverGameObject(i32 pointerId)
    {
        PointerEventData* last = GetLastPointerEventData(pointerId);
        return last && last->pointerEnter();
    }

    void PointerInputModule::ClearSelection()
    {
        BaseEventData* base = GetBaseEventData();
        for (Pointer& p : pointerData_)
            HandlePointerExitAndEnter(p.data, nullptr);
        for (Pointer& p : pointerData_)
            if (p.data->Wrapper() == 0)
                delete p.data;
        pointerData_.Clear();
        if (EventSystem* s = eventSystem())
            s->SetSelectedGameObject(nullptr, base);
    }

    void PointerInputModule::DeselectIfSelectionChanged(engine::GameObject* currentOverGo, BaseEventData* pointerEvent)
    {
        engine::GameObject* select = ExecuteEvents::GetEventHandlerInternal(currentOverGo, EventKind::Select);
        if (select != eventSystem()->currentSelectedGameObject())
            eventSystem()->SetSelectedGameObject(nullptr, pointerEvent);
    }

    String PointerInputModule::Describe() const
    {
        String s = Format("<b>Pointer Input Module of type: </b>{}\n", script() ? StringView(script()->name) : StringView(""));
        for (const Pointer& p : pointerData_)
            s += Format("<B>Pointer:</b> {}\n{}\n", p.id, p.data->Describe());
        return s;
    }

    // ------------------------------------------------------------------------------------- StandaloneInputModule

    bool StandaloneInputModule::ShouldIgnoreEventsOnNoFocus() const { return true; }

    void StandaloneInputModule::UpdateModule()
    {
        EventSystem* s = eventSystem();
        if (s && !s->isFocused() && ShouldIgnoreEventsOnNoFocus())
        {
            if (inputPointerEvent_ && inputPointerEvent_->pointerDrag() && inputPointerEvent_->dragging())
                ReleaseMouse(inputPointerEvent_, inputPointerEvent_->pointerCurrentRaycast.gameObject.Get());
            inputPointerEvent_ = nullptr;
            return;
        }
        lastMousePosition_ = mousePosition_;
        mousePosition_ = input()->mousePosition();
    }

    void StandaloneInputModule::ReleaseMouse(PointerEventData* e, engine::GameObject* currentOverGo)
    {
        ExecuteEvents::ExecuteInternal(e->pointerPress(), e, EventKind::PointerUp);
        engine::GameObject* clickHandler = ExecuteEvents::GetEventHandlerInternal(currentOverGo, EventKind::PointerClick);
        if (e->pointerClick() == clickHandler && e->eligibleForClick())
            ExecuteEvents::ExecuteInternal(e->pointerClick(), e, EventKind::PointerClick);
        if (e->pointerDrag() && e->dragging())
            ExecuteEvents::ExecuteHierarchyInternal(currentOverGo, e, EventKind::Drop);
        e->SetEligibleForClick(false);
        e->SetPointerPress(nullptr);
        e->SetRawPointerPress(nullptr);
        e->SetPointerClick(nullptr);
        if (e->pointerDrag() && e->dragging())
            ExecuteEvents::ExecuteInternal(e->pointerDrag(), e, EventKind::EndDrag);
        e->SetDragging(false);
        e->SetPointerDrag(nullptr);
        // Enter and exit again: what was skipped while pressed hears it now.
        if (currentOverGo != e->pointerEnter())
        {
            HandlePointerExitAndEnter(e, nullptr);
            HandlePointerExitAndEnter(e, currentOverGo);
        }
        inputPointerEvent_ = e;
    }

    bool StandaloneInputModule::ShouldActivateModule()
    {
        if (!InputModule::ShouldActivateModule())
            return false;
        BaseInput* in = input();
        bool activate = forceModuleActive_;
        activate = activate || in->GetButtonDown(submitButton_) || in->GetButtonDown(cancelButton_);
        activate = activate || std::fabs(in->GetAxisRaw(horizontalAxis_)) > 1e-6f || std::fabs(in->GetAxisRaw(verticalAxis_)) > 1e-6f;
        Vector2 moved(mousePosition_.x - lastMousePosition_.x, mousePosition_.y - lastMousePosition_.y);
        activate = activate || moved.x * moved.x + moved.y * moved.y > 0 || in->GetMouseButtonDown(0);
        return activate || in->touchCount() > 0;
    }

    void StandaloneInputModule::ActivateModule()
    {
        EventSystem* s = eventSystem();
        if (!s || (!s->isFocused() && ShouldIgnoreEventsOnNoFocus()))
            return;
        InputModule::ActivateModule();
        mousePosition_ = input()->mousePosition();
        lastMousePosition_ = mousePosition_;
        engine::GameObject* select = s->currentSelectedGameObject();
        if (!select)
            select = s->firstSelectedGameObject();
        s->SetSelectedGameObject(select, GetBaseEventData());
    }

    void StandaloneInputModule::Process()
    {
        EventSystem* s = eventSystem();
        if (!s || (!s->isFocused() && ShouldIgnoreEventsOnNoFocus()))
            return;
        bool used = SendUpdateEventToSelectedObject();
        // Touches before the mouse (the mouse emulation follows them), both before navigation.
        if (!ProcessTouchEvents() && input()->mousePresent())
            ProcessMouseEvent();
        if (s->sendNavigationEvents())
        {
            if (!used)
                used = SendMoveEventToSelectedObject();
            if (!used)
                SendSubmitEventToSelectedObject();
        }
    }

    bool StandaloneInputModule::ProcessTouchEvents()
    {
        BaseInput* in = input();
        for (i32 i = 0; i < in->touchCount(); i++)
        {
            engine::Touch touch = in->GetTouch(i);
            if (touch.type == engine::TouchType::Indirect)
                continue;
            bool pressed = false, released = false;
            PointerEventData* pointer = GetTouchPointerEventData(touch, pressed, released);
            ProcessTouchPress(pointer, pressed, released);
            if (!released)
            {
                ProcessMove(pointer);
                ProcessDrag(pointer);
            }
            else
                RemovePointerData(pointer);
        }
        return in->touchCount() > 0;
    }

    namespace
    {
        // The press of a pointer: selection, down (to the handler up the hierarchy, or the click handler), click counts,
        // the drag handler's initialize.
        void Press(PointerInputModule& m, PointerEventData* e, engine::GameObject* currentOverGo, void (PointerInputModule::*deselect)(engine::GameObject*, BaseEventData*))
        {
            e->SetEligibleForClick(true);
            e->SetDelta(Vector2(0, 0));
            e->SetDragging(false);
            e->SetUseDragThreshold(true);
            e->SetPressPosition(e->position());
            e->pointerPressRaycast = e->pointerCurrentRaycast;
            (m.*deselect)(currentOverGo, e);
            f32 now = engine::Time::unscaledTime();
            if (now - e->clickTime() >= kDoubleClickTime)
                e->SetClickCount(0);
            engine::GameObject* pressed = ExecuteEvents::ExecuteHierarchyInternal(currentOverGo, e, EventKind::PointerDown);
            engine::GameObject* click = ExecuteEvents::GetEventHandlerInternal(currentOverGo, EventKind::PointerClick);
            if (!pressed)
                pressed = click;
            if (pressed == e->lastPress())
            {
                e->SetClickCount(now - e->clickTime() < kDoubleClickTime ? e->clickCount() + 1 : 1);
                e->SetClickTime(now);
            }
            else
                e->SetClickCount(1);
            e->SetPointerPress(pressed);
            e->SetRawPointerPress(currentOverGo);
            e->SetPointerClick(click);
            e->SetClickTime(now);
            e->SetPointerDrag(ExecuteEvents::GetEventHandlerInternal(currentOverGo, EventKind::Drag));
            if (e->pointerDrag())
                ExecuteEvents::ExecuteInternal(e->pointerDrag(), e, EventKind::InitializePotentialDrag);
        }
    }

    void StandaloneInputModule::ProcessTouchPress(PointerEventData* e, bool pressed, bool released)
    {
        engine::GameObject* currentOverGo = e->pointerCurrentRaycast.gameObject.Get();
        if (pressed)
        {
            // The touched element hears an enter first.
            if (e->pointerEnter() != currentOverGo)
            {
                HandlePointerExitAndEnter(e, currentOverGo);
                e->SetPointerEnter(currentOverGo);
            }
            Press(*this, e, currentOverGo, &PointerInputModule::DeselectIfSelectionChanged);
        }
        if (released)
        {
            ExecuteEvents::ExecuteInternal(e->pointerPress(), e, EventKind::PointerUp);
            engine::GameObject* clickHandler = ExecuteEvents::GetEventHandlerInternal(currentOverGo, EventKind::PointerClick);
            if (e->pointerClick() == clickHandler && e->eligibleForClick())
                ExecuteEvents::ExecuteInternal(e->pointerClick(), e, EventKind::PointerClick);
            if (e->pointerDrag() && e->dragging())
                ExecuteEvents::ExecuteHierarchyInternal(currentOverGo, e, EventKind::Drop);
            e->SetEligibleForClick(false);
            e->SetPointerPress(nullptr);
            e->SetRawPointerPress(nullptr);
            e->SetPointerClick(nullptr);
            if (e->pointerDrag() && e->dragging())
                ExecuteEvents::ExecuteInternal(e->pointerDrag(), e, EventKind::EndDrag);
            e->SetDragging(false);
            e->SetPointerDrag(nullptr);
            // A touch that lifts leaves.
            ExecuteEvents::ExecuteHierarchyInternal(e->pointerEnter(), e, EventKind::PointerExit);
            e->SetPointerEnter(nullptr);
        }
        inputPointerEvent_ = e;
    }

    bool StandaloneInputModule::SendSubmitEventToSelectedObject()
    {
        EventSystem* s = eventSystem();
        if (!s->currentSelectedGameObject())
            return false;
        BaseEventData* data = GetBaseEventData();
        if (input()->GetButtonDown(submitButton_))
            ExecuteEvents::ExecuteInternal(s->currentSelectedGameObject(), data, EventKind::Submit);
        if (input()->GetButtonDown(cancelButton_))
            ExecuteEvents::ExecuteInternal(s->currentSelectedGameObject(), data, EventKind::Cancel);
        return data->used();
    }

    Vector2 StandaloneInputModule::RawNavigation()
    {
        BaseInput* in = input();
        Vector2 move(in->GetAxisRaw(horizontalAxis_), in->GetAxisRaw(verticalAxis_));
        if (in->GetButtonDown(horizontalAxis_))
            move.x = move.x < 0 ? -1.0f : move.x > 0 ? 1.0f : move.x;
        if (in->GetButtonDown(verticalAxis_))
            move.y = move.y < 0 ? -1.0f : move.y > 0 ? 1.0f : move.y;
        return move;
    }

    bool StandaloneInputModule::SendMoveEventToSelectedObject()
    {
        f32 now = engine::Time::unscaledTime();
        Vector2 movement = RawNavigation();
        if (std::fabs(movement.x) < 1e-6f && std::fabs(movement.y) < 1e-6f)
        {
            consecutiveMoveCount_ = 0;
            return false;
        }
        bool similar = movement.x * lastMoveVector_.x + movement.y * lastMoveVector_.y > 0;
        // The same way: a delay before the repeat; otherwise the repeat rate.
        if (similar && consecutiveMoveCount_ == 1)
        {
            if (now <= prevActionTime_ + repeatDelay_)
                return false;
        }
        else if (now <= prevActionTime_ + 1.0f / inputActionsPerSecond_)
            return false;
        AxisEventData* data = GetAxisEventData(movement.x, movement.y, 0.6f);
        if (data->moveDir() != MoveDirection::None)
        {
            ExecuteEvents::ExecuteInternal(eventSystem()->currentSelectedGameObject(), data, EventKind::Move);
            if (!similar)
                consecutiveMoveCount_ = 0;
            consecutiveMoveCount_++;
            prevActionTime_ = now;
            lastMoveVector_ = movement;
        }
        else
            consecutiveMoveCount_ = 0;
        return data->used();
    }

    void StandaloneInputModule::ProcessMouseEvent() { ProcessMouseEvent(0); }

    void StandaloneInputModule::ProcessMouseEvent(i32 id)
    {
        ButtonFrame buttons[3];
        MouseFrame(id, buttons);
        PointerEventData* left = buttons[0].data;
        currentFocusedGameObject_ = engine::PPtr<engine::GameObject>(left->pointerCurrentRaycast.gameObject.Get());
        ProcessMousePress(buttons[0]);
        ProcessMove(left);
        ProcessDrag(left);
        ProcessMousePress(buttons[1]);
        ProcessDrag(buttons[1].data);
        ProcessMousePress(buttons[2]);
        ProcessDrag(buttons[2].data);
        Vector2 scroll = left->scrollDelta();
        if (scroll.x * scroll.x + scroll.y * scroll.y > 1e-12f)
        {
            engine::GameObject* handler = ExecuteEvents::GetEventHandlerInternal(left->pointerCurrentRaycast.gameObject.Get(), EventKind::Scroll);
            ExecuteEvents::ExecuteHierarchyInternal(handler, left, EventKind::Scroll);
        }
    }

    bool StandaloneInputModule::SendUpdateEventToSelectedObject()
    {
        EventSystem* s = eventSystem();
        if (!s->currentSelectedGameObject())
            return false;
        BaseEventData* data = GetBaseEventData();
        ExecuteEvents::ExecuteInternal(s->currentSelectedGameObject(), data, EventKind::UpdateSelected);
        return data->used();
    }

    void StandaloneInputModule::ProcessMousePress(const ButtonFrame& data)
    {
        PointerEventData* e = data.data;
        engine::GameObject* currentOverGo = e->pointerCurrentRaycast.gameObject.Get();
        if (data.PressedThisFrame())
        {
            Press(*this, e, currentOverGo, &PointerInputModule::DeselectIfSelectionChanged);
            inputPointerEvent_ = e;
        }
        if (data.ReleasedThisFrame())
            ReleaseMouse(e, currentOverGo);
    }

    void StandaloneInputModule::ProcessMousePressInternal(PointerEventData* data, i32 state)
    {
        ButtonFrame f;
        f.data = data;
        f.state = PointerEventData::FramePressState(state);
        if (data)
            ProcessMousePress(f);
    }

    // ------------------------------------------------------------------------------------------ TouchInputModule

    void TouchInputModule::UpdateModule()
    {
        lastMousePosition_ = mousePosition_;
        mousePosition_ = input()->mousePosition();
    }

    bool TouchInputModule::IsModuleSupported() { return forceModuleActive_ || input()->touchSupported(); }

    bool TouchInputModule::ShouldActivateModule()
    {
        if (!InputModule::ShouldActivateModule())
            return false;
        if (forceModuleActive_)
            return true;
        if (!input()->touchSupported())
        {
            Vector2 moved(mousePosition_.x - lastMousePosition_.x, mousePosition_.y - lastMousePosition_.y);
            return input()->GetMouseButtonDown(0) || moved.x * moved.x + moved.y * moved.y > 0;
        }
        return input()->touchCount() > 0;
    }

    void TouchInputModule::Process()
    {
        BaseInput* in = input();
        if (!in->touchSupported())
        {
            // The mouse as a touch.
            ButtonFrame buttons[3];
            MouseFrame(0, buttons);
            ButtonFrame& left = buttons[0];
            if (left.PressedThisFrame())
                left.data->SetDelta(Vector2(0, 0));
            ProcessTouchPress(left.data, left.PressedThisFrame(), left.ReleasedThisFrame());
            if (in->GetMouseButton(0))
            {
                ProcessMove(left.data);
                ProcessDrag(left.data);
            }
            return;
        }
        for (i32 i = 0; i < in->touchCount(); i++)
        {
            engine::Touch touch = in->GetTouch(i);
            bool pressed = false, released = false;
            PointerEventData* pointer = GetTouchPointerEventData(touch, pressed, released);
            ProcessTouchPress(pointer, pressed, released);
            if (!released)
            {
                ProcessMove(pointer);
                ProcessDrag(pointer);
            }
            else
                RemovePointerData(pointer);
        }
    }

    void TouchInputModule::ProcessTouchPress(PointerEventData* e, bool pressed, bool released)
    {
        engine::GameObject* currentOverGo = e->pointerCurrentRaycast.gameObject.Get();
        if (pressed)
        {
            if (e->pointerEnter() != currentOverGo)
            {
                HandlePointerExitAndEnter(e, currentOverGo);
                e->SetPointerEnter(currentOverGo);
            }
            Press(*this, e, currentOverGo, &PointerInputModule::DeselectIfSelectionChanged);
        }
        if (released)
        {
            ExecuteEvents::ExecuteInternal(e->pointerPress(), e, EventKind::PointerUp);
            engine::GameObject* clickHandler = ExecuteEvents::GetEventHandlerInternal(currentOverGo, EventKind::PointerClick);
            if (e->pointerClick() == clickHandler && e->eligibleForClick())
                ExecuteEvents::ExecuteInternal(e->pointerClick(), e, EventKind::PointerClick);
            if (e->pointerDrag() && e->dragging())
                ExecuteEvents::ExecuteHierarchyInternal(currentOverGo, e, EventKind::Drop);
            e->SetEligibleForClick(false);
            e->SetPointerPress(nullptr);
            e->SetRawPointerPress(nullptr);
            e->SetPointerClick(nullptr);
            if (e->pointerDrag() && e->dragging())
                ExecuteEvents::ExecuteInternal(e->pointerDrag(), e, EventKind::EndDrag);
            e->SetDragging(false);
            e->SetPointerDrag(nullptr);
            ExecuteEvents::ExecuteHierarchyInternal(e->pointerEnter(), e, EventKind::PointerExit);
            e->SetPointerEnter(nullptr);
        }
    }

    void EventSystem::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        dummyData_ = nullptr;
        modules_.Clear();
    }

    void InputModule::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        axisEventData_ = nullptr;
        baseEventData_ = nullptr;
    }

    void PointerInputModule::OnAfterCloneNative()
    {
        InputModule::OnAfterCloneNative();
        pointerData_.Clear();
    }

    void StandaloneInputModule::OnAfterCloneNative()
    {
        PointerInputModule::OnAfterCloneNative();
        inputPointerEvent_ = nullptr;
        currentFocusedGameObject_ = engine::PPtr<engine::GameObject>();
    }
}
