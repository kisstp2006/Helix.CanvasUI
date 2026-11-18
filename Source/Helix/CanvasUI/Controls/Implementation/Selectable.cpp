#include <Helix/CanvasUI/Controls/Selectable.h>

#include <Helix/CanvasUI/Core/Implementation/CanvasUILog.h>
#include <Helix/Engine/Time/Time.h>

#include <cmath>
#include <limits>

namespace helix::canvasui
{
    namespace
    {
        // The enabled selectables, in the order they were enabled (a disabled one's place takes the last).
        Array<Selectable*>& All()
        {
            static Array<Selectable*>* a = new Array<Selectable*>();
            return *a;
        }

        engine::EntityId IdOf(const engine::PPtr<engine::Object>& p) { return p.Get() ? p.Get()->GetEntityId() : engine::EntityId(); }
        engine::EntityId IdOf(const engine::PPtr<engine::Sprite>& p) { return p.Get() ? p.Get()->GetEntityId() : engine::EntityId(); }
    }

    // ------------------------------------------------------------------------------------------------- Selectable

    Selectable::Selectable() { triggers_ = new AnimationTriggers(&triggerFields_); }

    Selectable::~Selectable()
    {
        // A C# object of the triggers lives on as its own.
        if (triggers_ && triggers_->Wrapper() == 0)
            delete triggers_;
    }

    Array<Selectable*> Selectable::allSelectablesInternal() { return All(); }
    i32 Selectable::allSelectableCount() { return i32(All().Size()); }

    NavigationData Selectable::NavigationOf() const
    {
        NavigationData d;
        d.mode = navigation_.mode, d.wrapAround = navigation_.wrapAround;
        d.selectOnUp = IdOf(navigation_.up), d.selectOnDown = IdOf(navigation_.down);
        d.selectOnLeft = IdOf(navigation_.left), d.selectOnRight = IdOf(navigation_.right);
        return d;
    }

    void Selectable::SetNavigationOf(NavigationData value)
    {
        navigation_.mode = value.mode, navigation_.wrapAround = value.wrapAround;
        navigation_.up = engine::PPtr<engine::Object>(engine::EntityIdToObject(value.selectOnUp));
        navigation_.down = engine::PPtr<engine::Object>(engine::EntityIdToObject(value.selectOnDown));
        navigation_.left = engine::PPtr<engine::Object>(engine::EntityIdToObject(value.selectOnLeft));
        navigation_.right = engine::PPtr<engine::Object>(engine::EntityIdToObject(value.selectOnRight));
        OnSetProperty();
    }

    void Selectable::SetTransition(Transition value)
    {
        if (transition_ == value)
            return;
        transition_ = value;
        OnSetProperty();
    }

    void Selectable::SetColors(ColorBlockData value)
    {
        colors_.colors = value;
        OnSetProperty();
    }

    SpriteStateData Selectable::SpriteStateOf() const
    {
        SpriteStateData d;
        d.highlightedSprite = IdOf(spriteState_.highlighted), d.pressedSprite = IdOf(spriteState_.pressed);
        d.selectedSprite = IdOf(spriteState_.selected), d.disabledSprite = IdOf(spriteState_.disabled);
        return d;
    }

    void Selectable::SetSpriteStateOf(SpriteStateData value)
    {
        spriteState_.highlighted = engine::PPtr<engine::Sprite>(engine::EntityIdToObject<engine::Sprite>(value.highlightedSprite));
        spriteState_.pressed = engine::PPtr<engine::Sprite>(engine::EntityIdToObject<engine::Sprite>(value.pressedSprite));
        spriteState_.selected = engine::PPtr<engine::Sprite>(engine::EntityIdToObject<engine::Sprite>(value.selectedSprite));
        spriteState_.disabled = engine::PPtr<engine::Sprite>(engine::EntityIdToObject<engine::Sprite>(value.disabledSprite));
        OnSetProperty();
    }

    void Selectable::SetAnimationTriggers(AnimationTriggers* value)
    {
        if (value)
            triggerFields_ = value->Fields();
        OnSetProperty();
    }

    Graphic* Selectable::targetGraphic() const { return static_cast<Graphic*>(targetGraphic_.Get()); }

    void Selectable::SetTargetGraphic(Graphic* value)
    {
        if (targetGraphic_.Get() == value)
            return;
        targetGraphic_ = engine::PPtr<engine::Object>(value);
        OnSetProperty();
    }

    Image* Selectable::image() const
    {
        Graphic* g = targetGraphic();
        return g && g->script() && scripting::IsAssignableFrom(scripting::TypeOf<Image>(), g->script()->type) ? static_cast<Image*>(g) : nullptr;
    }

    void Selectable::SetInteractable(bool value)
    {
        if (interactable_ == value)
            return;
        interactable_ = value;
        EventSystem* es = EventSystem::current();
        if (!interactable_ && es && es->currentSelectedGameObject() == gameObject())
            es->SetSelectedGameObject(nullptr);
        OnSetProperty();
    }

    Selectable::SelectionState Selectable::currentSelectionState() const
    {
        if (!IsInteractable())
            return SelectionState::Disabled;
        if (isPointerDown_)
            return SelectionState::Pressed;
        if (hasSelection_)
            return SelectionState::Selected;
        if (isPointerInside_)
            return SelectionState::Highlighted;
        return SelectionState::Normal;
    }

    bool Selectable::IsHighlighted() const { return IsActive() && IsInteractable() && isPointerInside_ && !isPointerDown_ && !hasSelection_; }
    bool Selectable::IsPressed() const { return IsActive() && IsInteractable() && isPointerDown_; }

    bool Selectable::ParentGroupAllowsInteraction() const
    {
        for (engine::Transform* t = engine::Component::transform(); t; t = t->parent())
        {
            engine::GameObject* g = t->gameObject();
            for (u32 i = 0; g && i < g->GetComponentCount(); i++)
            {
                engine::Component* c = g->GetComponentAtIndex(i);
                if (!c || !c->Is<engine::CanvasGroup>())
                    continue;
                auto* group = static_cast<engine::CanvasGroup*>(c);
                if (group->enabled() && !group->interactable())
                    return false;
                if (group->ignoreParentGroups())
                    return true;
            }
        }
        return true;
    }

    void Selectable::OnCanvasGroupChanged()
    {
        bool allows = ParentGroupAllowsInteraction();
        if (allows != groupsAllowInteraction_)
        {
            groupsAllowInteraction_ = allows;
            OnSetProperty();
        }
    }

    void Selectable::OnTransformParentChanged()
    {
        UIBehaviour::OnTransformParentChanged();
        OnCanvasGroupChanged();
    }

    void Selectable::Awake()
    {
        if (!targetGraphic_.Get())
            targetGraphic_ = engine::PPtr<engine::Object>(ComponentOf<Graphic>(gameObject()));
    }

    void Selectable::Reset() { targetGraphic_ = engine::PPtr<engine::Object>(ComponentOf<Graphic>(gameObject())); }

    void Selectable::OnEnable()
    {
        if (registration_.enabled)
            return;
        UIBehaviour::OnEnable();
        registration_.index = i32(All().Size());
        All().Push(this);
        isPointerDown_ = false;
        groupsAllowInteraction_ = ParentGroupAllowsInteraction();
        DoStateTransition(currentSelectionState(), true);
        registration_.enabled = true;
    }

    void Selectable::OnDisable()
    {
        if (!registration_.enabled)
            return;
        Array<Selectable*>& all = All();
        // The last one takes this one's place.
        Selectable* last = all[all.Size() - 1];
        last->registration_.index = registration_.index;
        all[usize(registration_.index)] = last;
        all.RemoveAt(all.Size() - 1);
        InstantClearState();
        UIBehaviour::OnDisable();
        registration_.enabled = false;
    }

    void Selectable::OnApplicationFocus(bool hasFocus)
    {
        if (!hasFocus && IsPressed())
            InstantClearState();
    }

    void Selectable::InstantClearState()
    {
        isPointerInside_ = isPointerDown_ = hasSelection_ = false;
        switch (transition_)
        {
        case Transition::ColorTint: StartColorTween(Color(1, 1, 1, 1), true); break;
        case Transition::SpriteSwap: DoSpriteSwap(nullptr); break;
        // The animator's triggers play with the animation step.
        case Transition::Animation:
        case Transition::None: break;
        }
    }

    void Selectable::DoStateTransition(SelectionState state, bool instant)
    {
        engine::GameObject* g = gameObject();
        if (!g || !g->activeInHierarchy())
            return;
        const ColorBlockData& c = colors_.colors;
        Color tint(0, 0, 0, 1);
        engine::Sprite* sprite = nullptr;
        switch (state)
        {
        case SelectionState::Normal: tint = c.normalColor; break;
        case SelectionState::Highlighted:
            tint = c.highlightedColor;
            sprite = spriteState_.highlighted.Get();
            break;
        case SelectionState::Pressed:
            tint = c.pressedColor;
            sprite = spriteState_.pressed.Get();
            break;
        case SelectionState::Selected:
            tint = c.selectedColor;
            sprite = spriteState_.selected.Get();
            break;
        case SelectionState::Disabled:
            tint = c.disabledColor;
            sprite = spriteState_.disabled.Get();
            break;
        }
        switch (transition_)
        {
        case Transition::ColorTint: StartColorTween(tint * c.colorMultiplier, instant); break;
        case Transition::SpriteSwap: DoSpriteSwap(sprite); break;
        case Transition::Animation:
        case Transition::None: break;
        }
    }

    void Selectable::StartColorTween(Color target, bool instant)
    {
        if (Graphic* g = targetGraphic())
            g->CrossFadeColor(target, instant ? 0.0f : colors_.colors.fadeDuration, true, true);
    }

    void Selectable::DoSpriteSwap(engine::Sprite* sprite)
    {
        if (Image* i = image())
            i->SetOverrideSprite(sprite);
    }

    void Selectable::EvaluateAndTransitionToSelectionState()
    {
        if (!IsActive() || !IsInteractable())
            return;
        DoStateTransition(currentSelectionState(), false);
    }

    namespace
    {
        // A point on the rectangle's edge that way (dir scaled to the edge from the center).
        Vector3 PointOnRectEdge(engine::RectTransform* rect, Vector2 dir)
        {
            if (!rect)
                return Vector3(0, 0, 0);
            f32 m = std::fabs(dir.x) > std::fabs(dir.y) ? std::fabs(dir.x) : std::fabs(dir.y);
            if (m > 0)
                dir = Vector2(dir.x / m, dir.y / m);
            Rect r = rect->rect();
            return Vector3(r.x + r.width * 0.5f + r.width * dir.x * 0.5f, r.y + r.height * 0.5f + r.height * dir.y * 0.5f, 0);
        }
    }

    Selectable* Selectable::FindSelectable(Vector3 dir)
    {
        dir = dir.normalized();
        engine::Transform* t = engine::Component::transform();
        Vector3 localDir = Quaternion::Inverse(t->rotation()) * dir;
        engine::RectTransform* rt = t->Is<engine::RectTransform>() ? static_cast<engine::RectTransform*>(t) : nullptr;
        Vector3 pos = t->TransformPoint(PointOnRectEdge(rt, Vector2(localDir.x, localDir.y)));
        f32 maxScore = -std::numeric_limits<f32>::infinity(), maxFurthest = -std::numeric_limits<f32>::infinity();
        bool wrap = navigation_.wrapAround && (navigation_.mode == NavigationMode::Vertical || navigation_.mode == NavigationMode::Horizontal);
        Selectable *best = nullptr, *furthest = nullptr;
        for (Selectable* sel : All())
        {
            if (sel == this || !sel->IsInteractable() || sel->navigation_.mode == NavigationMode::None)
                continue;
            engine::Transform* st = sel->engine::Component::transform();
            Vector3 center(0, 0, 0);
            if (st->Is<engine::RectTransform>())
            {
                Rect r = static_cast<engine::RectTransform*>(st)->rect();
                center = Vector3(r.x + r.width * 0.5f, r.y + r.height * 0.5f, 0);
            }
            Vector3 v = st->TransformPoint(center) - pos;
            f32 dot = Vector3::Dot(dir, v);
            f32 sq = v.x * v.x + v.y * v.y + v.z * v.z;
            // Behind with wrap around: the furthest that way is the candidate.
            if (wrap && dot < 0)
            {
                f32 score = -dot * sq;
                if (score > maxFurthest)
                    maxFurthest = score, furthest = sel;
                continue;
            }
            if (dot <= 0)
                continue;
            // Nearer and more in the direction score higher (dot over the squared distance).
            f32 score = dot / sq;
            if (score > maxScore)
                maxScore = score, best = sel;
        }
        if (wrap && !best)
            return furthest;
        return best;
    }

    Selectable* Selectable::FindSelectableOnLeft()
    {
        if (navigation_.mode == NavigationMode::Explicit)
            return static_cast<Selectable*>(navigation_.left.Get());
        if ((i32(navigation_.mode) & i32(NavigationMode::Horizontal)) != 0)
            return FindSelectable(engine::Component::transform()->rotation() * Vector3(-1, 0, 0));
        return nullptr;
    }

    Selectable* Selectable::FindSelectableOnRight()
    {
        if (navigation_.mode == NavigationMode::Explicit)
            return static_cast<Selectable*>(navigation_.right.Get());
        if ((i32(navigation_.mode) & i32(NavigationMode::Horizontal)) != 0)
            return FindSelectable(engine::Component::transform()->rotation() * Vector3(1, 0, 0));
        return nullptr;
    }

    Selectable* Selectable::FindSelectableOnUp()
    {
        if (navigation_.mode == NavigationMode::Explicit)
            return static_cast<Selectable*>(navigation_.up.Get());
        if ((i32(navigation_.mode) & i32(NavigationMode::Vertical)) != 0)
            return FindSelectable(engine::Component::transform()->rotation() * Vector3(0, 1, 0));
        return nullptr;
    }

    Selectable* Selectable::FindSelectableOnDown()
    {
        if (navigation_.mode == NavigationMode::Explicit)
            return static_cast<Selectable*>(navigation_.down.Get());
        if ((i32(navigation_.mode) & i32(NavigationMode::Vertical)) != 0)
            return FindSelectable(engine::Component::transform()->rotation() * Vector3(0, -1, 0));
        return nullptr;
    }

    void Selectable::Navigate(AxisEventData* eventData, Selectable* sel)
    {
        if (sel && sel->IsActive())
            eventData->SetSelectedObject(sel->gameObject());
    }

    void Selectable::OnMove(AxisEventData* eventData)
    {
        switch (eventData->moveDir())
        {
        case MoveDirection::Right: Navigate(eventData, FindSelectableOnRight()); break;
        case MoveDirection::Up: Navigate(eventData, FindSelectableOnUp()); break;
        case MoveDirection::Left: Navigate(eventData, FindSelectableOnLeft()); break;
        case MoveDirection::Down: Navigate(eventData, FindSelectableOnDown()); break;
        case MoveDirection::None: break;
        }
    }

    void Selectable::OnPointerDown(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left)
            return;
        EventSystem* es = EventSystem::current();
        if (IsInteractable() && navigation_.mode != NavigationMode::None && es)
            es->SetSelectedGameObject(gameObject(), eventData);
        isPointerDown_ = true;
        EvaluateAndTransitionToSelectionState();
    }

    void Selectable::OnPointerUp(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left)
            return;
        isPointerDown_ = false;
        EvaluateAndTransitionToSelectionState();
    }

    void Selectable::OnPointerEnter(PointerEventData* eventData)
    {
        (void)eventData;
        isPointerInside_ = true;
        EvaluateAndTransitionToSelectionState();
    }

    void Selectable::OnPointerExit(PointerEventData* eventData)
    {
        (void)eventData;
        isPointerInside_ = false;
        EvaluateAndTransitionToSelectionState();
    }

    void Selectable::OnSelect(BaseEventData* eventData)
    {
        (void)eventData;
        hasSelection_ = true;
        EvaluateAndTransitionToSelectionState();
    }

    void Selectable::OnDeselect(BaseEventData* eventData)
    {
        (void)eventData;
        hasSelection_ = false;
        EvaluateAndTransitionToSelectionState();
    }

    void Selectable::Select()
    {
        EventSystem* es = EventSystem::current();
        if (!es || es->alreadySelecting())
            return;
        es->SetSelectedGameObject(gameObject());
    }

    bool Selectable::Handles(EventKind kind) const
    {
        switch (kind)
        {
        case EventKind::Move:
        case EventKind::PointerDown:
        case EventKind::PointerUp:
        case EventKind::PointerEnter:
        case EventKind::PointerExit:
        case EventKind::Select:
        case EventKind::Deselect: return true;
        default: return HandlesMore(kind);
        }
    }

    void Selectable::HandleEvent(EventKind kind, BaseEventData* data)
    {
        switch (kind)
        {
        case EventKind::Move: OnMove(static_cast<AxisEventData*>(data)); break;
        case EventKind::PointerDown: OnPointerDown(static_cast<PointerEventData*>(data)); break;
        case EventKind::PointerUp: OnPointerUp(static_cast<PointerEventData*>(data)); break;
        case EventKind::PointerEnter: OnPointerEnter(static_cast<PointerEventData*>(data)); break;
        case EventKind::PointerExit: OnPointerExit(static_cast<PointerEventData*>(data)); break;
        case EventKind::Select: OnSelect(data); break;
        case EventKind::Deselect: OnDeselect(data); break;
        default: HandleMore(kind, data); break;
        }
    }

    // ----------------------------------------------------------------------------------------------------- Button

    namespace
    {
        Array<engine::PPtr<engine::Object>>& Submits()
        {
            static Array<engine::PPtr<engine::Object>>* s = new Array<engine::PPtr<engine::Object>>();
            return *s;
        }
    }

    Button::Button()
    {
        static bool registered = false;
        if (!registered)
        {
            registered = true;
            engine::NativeEventEntry e;
            e.storage = [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<Button*>(o)->onClick_; };
            e.raise = [](engine::Object* o, Span<const scripting::Variant>) { static_cast<Button*>(o)->onClick_.Invoke(); };
            engine::RegisterNativeEvent(scripting::TypeOf<Button>(), 0, e);
        }
    }

    void Button::Press()
    {
        if (!IsActive() || !IsInteractable())
            return;
        engine::UISystemProfilerApi::AddMarker("Button.onClick", this);
        onClick_.Invoke();
    }

    void Button::OnPointerClick(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left)
            return;
        Press();
    }

    void Button::OnSubmit(BaseEventData* eventData)
    {
        (void)eventData;
        Press();
        // Disabled by the press: no pressed state.
        if (!IsActive() || !IsInteractable())
            return;
        DoStateTransition(SelectionState::Pressed, false);
        submitElapsed_ = 0;
        Submits().Push(engine::PPtr<engine::Object>(this));
        static bool hooked = false;
        if (!hooked)
        {
            hooked = true;
            engine::Canvas::preWillRenderCanvasesInternal().Add([](void*) { Button::UpdateSubmits(); });
        }
    }

    void Button::UpdateSubmits()
    {
        Array<engine::PPtr<engine::Object>>& list = Submits();
        for (usize i = 0; i < list.Size();)
        {
            auto* b = static_cast<Button*>(list[i].Get());
            if (!b || !b->gameObject() || !b->gameObject()->activeInHierarchy())
            {
                list.RemoveAt(i);
                continue;
            }
            // The pressed look for the fade's time, then the state it is in now.
            if (b->submitElapsed_ < b->colors().fadeDuration)
            {
                b->submitElapsed_ += engine::Time::unscaledDeltaTime();
                i++;
                continue;
            }
            b->DoStateTransition(b->currentSelectionState(), false);
            b->submitElapsed_ = -1;
            list.RemoveAt(i);
        }
    }

    void Button::HandleMore(EventKind kind, BaseEventData* data)
    {
        if (kind == EventKind::PointerClick)
            OnPointerClick(static_cast<PointerEventData*>(data));
        else if (kind == EventKind::Submit)
            OnSubmit(data);
    }

    // ----------------------------------------------------------------------------------------------------- Toggle

    Toggle::Toggle()
    {
        static bool registered = false;
        if (!registered)
        {
            registered = true;
            engine::NativeEventEntry e;
            e.storage = [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<Toggle*>(o)->onValueChanged_; };
            e.raise = [](engine::Object* o, Span<const scripting::Variant> a) {
                bool v = false;
                if (!a.Empty())
                    a[0].Get(v);
                static_cast<Toggle*>(o)->onValueChanged_.Invoke(v);
            };
            engine::RegisterNativeEvent(scripting::TypeOf<Toggle>(), 0, e);
        }
    }

    Graphic* Toggle::graphic() const { return static_cast<Graphic*>(graphic_.Get()); }

    void Toggle::SetGraphic(Graphic* value) { graphic_ = engine::PPtr<engine::Object>(value); }

    ToggleGroup* Toggle::group() const { return static_cast<ToggleGroup*>(group_.Get()); }

    void Toggle::SetGroup(ToggleGroup* value)
    {
        SetToggleGroup(value, true);
        PlayEffect(true);
    }

    void Toggle::SetToggleGroup(ToggleGroup* newGroup, bool setMember)
    {
        if (ToggleGroup* g = group())
            g->UnregisterToggle(this);
        if (setMember)
            group_ = engine::PPtr<engine::Object>(newGroup);
        if (newGroup && IsActive())
            newGroup->RegisterToggle(this);
        if (newGroup && isOn_ && IsActive())
            newGroup->NotifyToggleOn(this);
    }

    void Toggle::Set(bool value, bool sendCallback)
    {
        if (isOn_ == value)
            return;
        isOn_ = value;
        ToggleGroup* g = group();
        if (g && g->isActiveAndEnabled() && IsActive())
            if (isOn_ || (!g->AnyTogglesOn() && !g->allowSwitchOff()))
            {
                isOn_ = true;
                g->NotifyToggleOn(this, sendCallback);
            }
        PlayEffect(toggleTransitionField == ToggleTransition::None);
        if (sendCallback)
        {
            engine::UISystemProfilerApi::AddMarker("Toggle.value", this);
            onValueChanged_.Invoke(isOn_);
        }
    }

    void Toggle::SetFromGroup(bool value, bool sendCallback) { Set(value, sendCallback); }

    void Toggle::PlayEffect(bool instant)
    {
        if (Graphic* g = graphic())
            g->CrossFadeAlpha(isOn_ ? 1.0f : 0.0f, instant ? 0.0f : 0.1f, true);
    }

    void Toggle::InternalToggle()
    {
        if (!IsActive() || !IsInteractable())
            return;
        SetIsOn(!isOn_);
    }

    void Toggle::OnPointerClick(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left)
            return;
        InternalToggle();
    }

    void Toggle::OnSubmit(BaseEventData* eventData)
    {
        (void)eventData;
        InternalToggle();
    }

    void Toggle::HandleMore(EventKind kind, BaseEventData* data)
    {
        if (kind == EventKind::PointerClick)
            OnPointerClick(static_cast<PointerEventData*>(data));
        else if (kind == EventKind::Submit)
            OnSubmit(data);
    }

    void Toggle::OnEnable()
    {
        Selectable::OnEnable();
        SetToggleGroup(group(), false);
        PlayEffect(true);
    }

    void Toggle::OnDisable()
    {
        SetToggleGroup(nullptr, false);
        Selectable::OnDisable();
    }

    void Toggle::OnDestroy()
    {
        if (ToggleGroup* g = group())
            g->EnsureValidState();
        Selectable::OnDestroy();
    }

    void Toggle::OnDidApplyAnimationProperties()
    {
        // An animation that changed the graphic's alpha changed the value.
        if (Graphic* g = graphic())
        {
            bool shown = std::fabs(g->canvasRenderer()->GetColor().a) > 1e-6f;
            if (isOn_ != shown)
            {
                isOn_ = shown;
                Set(!shown, true);
            }
        }
        Selectable::OnDidApplyAnimationProperties();
    }

    // ------------------------------------------------------------------------------------------------ ToggleGroup

    void ToggleGroup::Start()
    {
        EnsureValidState();
        UIBehaviour::Start();
    }

    void ToggleGroup::OnEnable()
    {
        EnsureValidState();
        UIBehaviour::OnEnable();
    }

    Array<Toggle*> ToggleGroup::TogglesInternal() const
    {
        Array<Toggle*> out;
        for (const engine::PPtr<engine::Object>& t : toggles_)
            if (engine::Object* o = t.Get())
                out.Push(static_cast<Toggle*>(o));
        return out;
    }

    void ToggleGroup::NotifyToggleOn(Toggle* toggle, bool sendCallback)
    {
        bool member = false;
        for (Toggle* t : TogglesInternal())
            member = member || t == toggle;
        if (!toggle || !member)
        {
            HX_LOG_ERROR(CanvasUILog, "Toggle {} is not part of ToggleGroup {}", toggle ? toggle->Name() : StringView("null"), Name());
            return;
        }
        for (Toggle* t : TogglesInternal())
            if (t != toggle)
            {
                if (sendCallback)
                    t->SetIsOn(false);
                else
                    t->SetIsOnWithoutNotify(false);
            }
    }

    void ToggleGroup::UnregisterToggle(Toggle* toggle)
    {
        for (usize i = 0; i < toggles_.Size(); i++)
            if (toggles_[i].Get() == toggle)
            {
                toggles_.RemoveAt(i);
                return;
            }
    }

    void ToggleGroup::RegisterToggle(Toggle* toggle)
    {
        for (const engine::PPtr<engine::Object>& t : toggles_)
            if (t.Get() == toggle)
                return;
        toggles_.Push(engine::PPtr<engine::Object>(toggle));
    }

    bool ToggleGroup::AnyTogglesOn() const
    {
        for (Toggle* t : TogglesInternal())
            if (t->isOn())
                return true;
        return false;
    }

    Array<Toggle*> ToggleGroup::ActiveTogglesInternal() const
    {
        Array<Toggle*> out;
        for (Toggle* t : TogglesInternal())
            if (t->isOn())
                out.Push(t);
        return out;
    }

    Toggle* ToggleGroup::GetFirstActiveToggle() const
    {
        Array<Toggle*> active = ActiveTogglesInternal();
        return active.Empty() ? nullptr : active[0];
    }

    void ToggleGroup::EnsureValidState()
    {
        Array<Toggle*> all = TogglesInternal();
        if (!allowSwitchOff_ && !AnyTogglesOn() && !all.Empty())
        {
            all[0]->SetIsOn(true);
            NotifyToggleOn(all[0]);
        }
        Array<Toggle*> active = ActiveTogglesInternal();
        if (active.Size() > 1)
        {
            Toggle* first = GetFirstActiveToggle();
            for (Toggle* t : active)
                if (t != first)
                    t->SetIsOn(false);
        }
    }

    void ToggleGroup::SetAllTogglesOff(bool sendCallback)
    {
        bool old = allowSwitchOff_;
        allowSwitchOff_ = true;
        for (Toggle* t : TogglesInternal())
        {
            if (sendCallback)
                t->SetIsOn(false);
            else
                t->SetIsOnWithoutNotify(false);
        }
        allowSwitchOff_ = old;
    }

    void Selectable::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        triggers_ = new AnimationTriggers(&triggerFields_);
    }

    void ToggleGroup::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        toggles_.Clear();
    }
}
