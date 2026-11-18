#pragma once

#include <Helix/CanvasUI/Events/EventSystem.h>
#include <Helix/CanvasUI/Rendering/Image.h>

// Selectable: the base of the controls. Its state (normal, highlighted, pressed, selected, disabled) comes from the
// pointer and the selection, gated by interactable and the CanvasGroups above; a transition shows it (the target
// graphic's color tint, a sprite swap, or animation triggers for an Animator). Navigation moves the selection between
// selectables (automatic by direction, or explicit). Button, Toggle and ToggleGroup are its simplest controls.
namespace helix::canvasui
{
    class Selectable;

    HX_ENUM(Namespace = "HelixEngine.CanvasUI", Internal)
    enum class NavigationMode : i32
    {
        None = 0,
        Horizontal = 1,
        Vertical = 2,
        Automatic = 3,
        Explicit = 4,
    };

    // Navigation across the bindings (C# has the struct Navigation over it): the selectables by their ids.
    HX_STRUCT(Namespace = "HelixEngine.CanvasUI", Internal)
    struct NavigationData
    {
        HX_FIELD() NavigationMode mode = NavigationMode::Automatic;
        HX_FIELD() bool wrapAround = false;
        HX_FIELD() engine::EntityId selectOnUp;
        HX_FIELD() engine::EntityId selectOnDown;
        HX_FIELD() engine::EntityId selectOnLeft;
        HX_FIELD() engine::EntityId selectOnRight;
    };

    HX_STRUCT(Namespace = "HelixEngine.CanvasUI", Internal)
    struct ColorBlockData
    {
        HX_FIELD() Color normalColor = Color(1, 1, 1, 1);
        HX_FIELD() Color highlightedColor = Color(245 / 255.0f, 245 / 255.0f, 245 / 255.0f, 1);
        HX_FIELD() Color pressedColor = Color(200 / 255.0f, 200 / 255.0f, 200 / 255.0f, 1);
        HX_FIELD() Color selectedColor = Color(245 / 255.0f, 245 / 255.0f, 245 / 255.0f, 1);
        HX_FIELD() Color disabledColor = Color(200 / 255.0f, 200 / 255.0f, 200 / 255.0f, 128 / 255.0f);
        HX_FIELD() f32 colorMultiplier = 1;
        HX_FIELD() f32 fadeDuration = 0.1f;
    };

    HX_STRUCT(Namespace = "HelixEngine.CanvasUI", Internal)
    struct SpriteStateData
    {
        HX_FIELD() engine::EntityId highlightedSprite;
        HX_FIELD() engine::EntityId pressedSprite;
        HX_FIELD() engine::EntityId selectedSprite;
        HX_FIELD() engine::EntityId disabledSprite;
    };

    // The selectable's settings as files hold them (the reference's m_ names).
    HX_STRUCT(SerializedOnly)
    struct NavigationField
    {
        static constexpr StringView kTypeName = "Navigation";
        NavigationMode mode = NavigationMode::Automatic;
        bool wrapAround = false;
        engine::PPtr<engine::Object> up, down, left, right;

        template <class V>
        void Transfer(V& v)
        {
            i32 m = i32(mode);
            v.Field("m_Mode", m);
            mode = NavigationMode(m);
            v.Field("m_WrapAround", wrapAround);
            v.Field("m_SelectOnUp", up);
            v.Field("m_SelectOnDown", down);
            v.Field("m_SelectOnLeft", left);
            v.Field("m_SelectOnRight", right);
        }
    };

    HX_STRUCT(SerializedOnly)
    struct ColorBlockField
    {
        static constexpr StringView kTypeName = "ColorBlock";
        ColorBlockData colors;

        template <class V>
        void Transfer(V& v)
        {
            v.Field("m_NormalColor", colors.normalColor);
            v.Field("m_HighlightedColor", colors.highlightedColor);
            v.Field("m_PressedColor", colors.pressedColor);
            v.Field("m_SelectedColor", colors.selectedColor);
            v.Field("m_DisabledColor", colors.disabledColor);
            v.Field("m_ColorMultiplier", colors.colorMultiplier);
            v.Field("m_FadeDuration", colors.fadeDuration);
        }
    };

    HX_STRUCT(SerializedOnly)
    struct SpriteStateField
    {
        static constexpr StringView kTypeName = "SpriteState";
        engine::PPtr<engine::Sprite> highlighted, pressed, selected, disabled;

        template <class V>
        void Transfer(V& v)
        {
            v.Field("m_HighlightedSprite", highlighted);
            v.Field("m_PressedSprite", pressed);
            v.Field("m_SelectedSprite", selected);
            v.Field("m_DisabledSprite", disabled);
        }
    };

    HX_STRUCT(SerializedOnly)
    struct TriggersField
    {
        static constexpr StringView kTypeName = "AnimationTriggers";
        String normalTrigger = String("Normal"), highlightedTrigger = String("Highlighted"), pressedTrigger = String("Pressed"),
               selectedTrigger = String("Selected"), disabledTrigger = String("Disabled");

        template <class V>
        void Transfer(V& v)
        {
            v.Field("m_NormalTrigger", normalTrigger);
            v.Field("m_HighlightedTrigger", highlightedTrigger);
            v.Field("m_PressedTrigger", pressedTrigger);
            v.Field("m_SelectedTrigger", selectedTrigger);
            v.Field("m_DisabledTrigger", disabledTrigger);
        }
    };

    // The names of the animator's triggers of each state (an Animator plays them with the animation step). A
    // selectable's own triggers are its object's; one made in C# has its own.
    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Root, Owned)
    class AnimationTriggers : public OwnedRoot<AnimationTriggers>
    {
    public:
        HX_FUNCTION(Constructor) AnimationTriggers() = default;
        explicit AnimationTriggers(TriggersField* fields) : fields_(fields) {}

        HX_PROPERTY() StringView normalTrigger() const { return Fields().normalTrigger; }
        void SetNormalTrigger(StringView value) { Fields().normalTrigger = String(value); }
        HX_PROPERTY() StringView highlightedTrigger() const { return Fields().highlightedTrigger; }
        void SetHighlightedTrigger(StringView value) { Fields().highlightedTrigger = String(value); }
        HX_PROPERTY() StringView pressedTrigger() const { return Fields().pressedTrigger; }
        void SetPressedTrigger(StringView value) { Fields().pressedTrigger = String(value); }
        HX_PROPERTY() StringView selectedTrigger() const { return Fields().selectedTrigger; }
        void SetSelectedTrigger(StringView value) { Fields().selectedTrigger = String(value); }
        HX_PROPERTY() StringView disabledTrigger() const { return Fields().disabledTrigger; }
        void SetDisabledTrigger(StringView value) { Fields().disabledTrigger = String(value); }
        TriggersField& Fields() const { return fields_ ? *fields_ : own_; }

        scripting::TypeId HelixType() const override { return scripting::TypeOf<AnimationTriggers>(); }

    private:
        TriggersField* fields_ = nullptr;
        mutable TriggersField own_;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class Selectable : public UIBehaviour, public INativeEventHandler, public IMoveHandler, public IPointerDownHandler, public IPointerUpHandler,
                       public IPointerEnterHandler, public IPointerExitHandler, public ISelectHandler, public IDeselectHandler
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        HX_ENUM()
        enum class Transition : i32
        {
            None = 0,
            ColorTint = 1,
            SpriteSwap = 2,
            Animation = 3,
        };
        HX_ENUM(Protected)
        enum class SelectionState : i32
        {
            Normal = 0,
            Highlighted = 1,
            Pressed = 2,
            Selected = 3,
            Disabled = 4,
        };

        Selectable();
        ~Selectable() override;

        HX_PROPERTY(ReadOnly, Internal) static Array<Selectable*> allSelectablesInternal();
        HX_PROPERTY(ReadOnly) static i32 allSelectableCount();

        HX_PROPERTY(Internal, Name = "navigationInternal") NavigationData NavigationOf() const;
        void SetNavigationOf(NavigationData value);
        HX_PROPERTY() Transition transition() const { return transition_; }
        void SetTransition(Transition value);
        HX_PROPERTY(Internal, Name = "colorsInternal") ColorBlockData colors() const { return colors_.colors; }
        void SetColors(ColorBlockData value);
        HX_PROPERTY(Internal, Name = "spriteStateInternal") SpriteStateData SpriteStateOf() const;
        void SetSpriteStateOf(SpriteStateData value);
        HX_PROPERTY() AnimationTriggers* animationTriggers() const { return triggers_; }
        void SetAnimationTriggers(AnimationTriggers* value);
        HX_PROPERTY() Graphic* targetGraphic() const;
        void SetTargetGraphic(Graphic* value);
        HX_PROPERTY() bool interactable() const { return interactable_; }
        void SetInteractable(bool value);
        HX_PROPERTY() Image* image() const;
        void SetImage(Image* value) { SetTargetGraphic(value); }
        HX_PROPERTY(ReadOnly, Protected) SelectionState currentSelectionState() const;

        HX_FUNCTION() virtual bool IsInteractable() const { return groupsAllowInteraction_ && interactable_; }
        HX_FUNCTION(Protected) bool IsHighlighted() const;
        HX_FUNCTION(Protected) bool IsPressed() const;
        // The best selectable that way from this one's edge: the nearest, the most in that direction.
        HX_FUNCTION() Selectable* FindSelectable(Vector3 dir);
        HX_FUNCTION() virtual Selectable* FindSelectableOnLeft();
        HX_FUNCTION() virtual Selectable* FindSelectableOnRight();
        HX_FUNCTION() virtual Selectable* FindSelectableOnUp();
        HX_FUNCTION() virtual Selectable* FindSelectableOnDown();
        HX_FUNCTION() void OnMove(AxisEventData* eventData) override;
        HX_FUNCTION(Protected) virtual void DoStateTransition(SelectionState state, bool instant);
        HX_FUNCTION(Protected) virtual void InstantClearState();
        HX_FUNCTION() void OnPointerDown(PointerEventData* eventData) override;
        HX_FUNCTION() void OnPointerUp(PointerEventData* eventData) override;
        HX_FUNCTION() void OnPointerEnter(PointerEventData* eventData) override;
        HX_FUNCTION() void OnPointerExit(PointerEventData* eventData) override;
        HX_FUNCTION() void OnSelect(BaseEventData* eventData) override;
        HX_FUNCTION() void OnDeselect(BaseEventData* eventData) override;
        HX_FUNCTION() virtual void Select();

        // INativeEventHandler: the handlers this class (and the classes below it) implement.
        bool Handles(EventKind kind) const override;
        void HandleEvent(EventKind kind, BaseEventData* data) override;
        INativeEventHandler* AsEventHandler() override { return this; }

        void Awake() override;
        void OnEnable() override;
        void OnDisable() override;
        void OnCanvasGroupChanged() override;
        void OnTransformParentChanged() override;
        void OnDidApplyAnimationProperties() override { OnSetProperty(); }
        void Reset() override;
        HX_FUNCTION(Protected) virtual void OnApplicationFocus(bool hasFocus);

    protected:
        void OnSetProperty() { DoStateTransition(currentSelectionState(), false); }
        void EvaluateAndTransitionToSelectionState();
        // Events of the subclasses' own handlers (Button: click and submit; Toggle the same; Slider, Scrollbar more).
        virtual bool HandlesMore(EventKind kind) const
        {
            (void)kind;
            return false;
        }
        virtual void HandleMore(EventKind kind, BaseEventData* data)
        {
            (void)kind, (void)data;
        }

        bool isPointerInside_ = false, isPointerDown_ = false, hasSelection_ = false;

    private:
        bool ParentGroupAllowsInteraction() const;
        void StartColorTween(Color target, bool instant);
        void DoSpriteSwap(engine::Sprite* sprite);
        void Navigate(AxisEventData* eventData, Selectable* sel);

        HX_FIELD(SerializeField, Name = "m_Navigation") NavigationField navigation_;
        HX_FIELD(SerializeField, Name = "m_Transition") Transition transition_ = Transition::ColorTint;
        HX_FIELD(SerializeField, Name = "m_Colors") ColorBlockField colors_;
        HX_FIELD(SerializeField, Name = "m_SpriteState") SpriteStateField spriteState_;
        HX_FIELD(SerializeField, Name = "m_AnimationTriggers") TriggersField triggerFields_;
        HX_FIELD(SerializeField, Name = "m_Interactable") bool interactable_ = true;
        HX_FIELD(SerializeField, Name = "m_TargetGraphic") engine::PPtr<engine::Object> targetGraphic_;
        AnimationTriggers* triggers_ = nullptr;
        bool groupsAllowInteraction_ = true;
        // The place in the list of the enabled selectables: a copy (Instantiate's) starts outside it.
        struct Registration
        {
            i32 index = -1;
            bool enabled = false;
            Registration() = default;
            Registration(const Registration&) {}
            Registration& operator=(const Registration&) { return *this; }
        };
        Registration registration_;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class Button : public Selectable, public IPointerClickHandler, public ISubmitHandler
    {
    public:
        // The click's event (files hold its persistent calls; C# has ButtonClickedEvent over it).
        HX_STRUCT(SerializedOnly)
        struct ButtonClickedEvent : engine::HelixEvent<>
        {
            static constexpr StringView kTypeName = "ButtonClickedEvent";
        };

        Button();
        HX_FUNCTION() virtual void OnPointerClick(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnSubmit(BaseEventData* eventData) override;
        HX_EVENT(Internal) engine::Event<>& onClickNative() { return onClick_.listeners(); }
        ButtonClickedEvent& onClick() { return onClick_; }

        // The submit's pressed state for a fade, then back (runs each frame while pending).
        static void UpdateSubmits();

    protected:
        bool HandlesMore(EventKind kind) const override { return kind == EventKind::PointerClick || kind == EventKind::Submit; }
        void HandleMore(EventKind kind, BaseEventData* data) override;

    private:
        void Press();

        HX_FIELD(SerializeField, Name = "m_OnClick") ButtonClickedEvent onClick_;
        f32 submitElapsed_ = -1;
    };

    class ToggleGroup;

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class Toggle : public Selectable, public IPointerClickHandler, public ISubmitHandler, public ICanvasElement
    {
    public:
        HX_ENUM()
        enum class ToggleTransition : i32
        {
            None = 0,
            Fade = 1,
        };
        HX_STRUCT(SerializedOnly)
        struct ToggleEvent : engine::HelixEvent<bool>
        {
            static constexpr StringView kTypeName = "ToggleEvent";
        };

        Toggle();
        HX_FIELD(Name = "toggleTransition", NonSerialized) ToggleTransition toggleTransitionField = ToggleTransition::Fade;
        HX_PROPERTY(Internal, Name = "graphicInternal") Graphic* graphic() const;
        void SetGraphic(Graphic* value);
        HX_PROPERTY() ToggleGroup* group() const;
        void SetGroup(ToggleGroup* value);
        HX_PROPERTY() bool isOn() const { return isOn_; }
        void SetIsOn(bool value) { Set(value, true); }
        HX_FUNCTION() void SetIsOnWithoutNotify(bool value) { Set(value, false); }
        HX_EVENT(Internal) engine::Event<bool>& onValueChangedNative() { return onValueChanged_.listeners(); }
        ToggleEvent& onValueChanged() { return onValueChanged_; }

        HX_FUNCTION() void Rebuild(CanvasUpdate executing) override { (void)executing; }
        HX_FUNCTION() void LayoutComplete() override {}
        HX_FUNCTION() void GraphicUpdateComplete() override {}
        engine::Transform* transform() const override { return engine::Component::transform(); }
        bool IsDestroyed() const override { return UIBehaviour::IsDestroyed(); }
        engine::Object* ElementObject() override { return this; }
        HX_FUNCTION() virtual void OnPointerClick(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnSubmit(BaseEventData* eventData) override;

        void Start() override { PlayEffect(true); }
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;
        void OnDidApplyAnimationProperties() override;

        // The group's own setting of the value (no group logic again).
        void SetFromGroup(bool value, bool sendCallback);

    protected:
        bool HandlesMore(EventKind kind) const override { return kind == EventKind::PointerClick || kind == EventKind::Submit; }
        void HandleMore(EventKind kind, BaseEventData* data) override;

    private:
        void Set(bool value, bool sendCallback);
        void PlayEffect(bool instant);
        void SetToggleGroup(ToggleGroup* group, bool setMember);
        void InternalToggle();

        HX_FIELD(SerializeField, Name = "graphic") engine::PPtr<engine::Object> graphic_;
        HX_FIELD(SerializeField, Name = "m_Group") engine::PPtr<engine::Object> group_;
        HX_FIELD(SerializeField, Name = "onValueChanged") ToggleEvent onValueChanged_;
        HX_FIELD(SerializeField, Name = "m_IsOn") bool isOn_ = false;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class ToggleGroup : public UIBehaviour
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        HX_PROPERTY() bool allowSwitchOff() const { return allowSwitchOff_; }
        void SetAllowSwitchOff(bool value) { allowSwitchOff_ = value; }
        HX_FUNCTION() void NotifyToggleOn(Toggle* toggle, bool sendCallback = true);
        HX_FUNCTION() void UnregisterToggle(Toggle* toggle);
        HX_FUNCTION() void RegisterToggle(Toggle* toggle);
        HX_FUNCTION() void EnsureValidState();
        HX_FUNCTION() bool AnyTogglesOn() const;
        HX_FUNCTION(Internal) Array<Toggle*> ActiveTogglesInternal() const;
        HX_FUNCTION() Toggle* GetFirstActiveToggle() const;
        HX_FUNCTION() void SetAllTogglesOff(bool sendCallback = true);
        HX_FUNCTION(Internal) Array<Toggle*> TogglesInternal() const;

        void Start() override;
        void OnEnable() override;

    private:
        Array<engine::PPtr<engine::Object>> toggles_;
        HX_FIELD(SerializeField, Name = "m_AllowSwitchOff") bool allowSwitchOff_ = false;
    };
}

HX_CANVASUI_ROOT_HANDLES(helix::canvasui::AnimationTriggers)
