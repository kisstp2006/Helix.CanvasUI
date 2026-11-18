#pragma once

#include <Helix/CanvasUI/Controls/Selectable.h>
#include <Helix/Engine/Script/Coroutine.h>
#include <Helix/Engine/Transform/RectTransform.h>

// The value controls: Slider (a value between a minimum and a maximum shown by a fill and a handle, dragged or moved
// by navigation) and Scrollbar (a handle of a size moved along its track; the scroll views' bars).
namespace helix::canvasui
{
    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::RectTransform")
    class Slider : public Selectable, public IDragHandler, public IInitializePotentialDragHandler, public ICanvasElement
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        HX_ENUM()
        enum class Direction : i32
        {
            LeftToRight = 0,
            RightToLeft = 1,
            BottomToTop = 2,
            TopToBottom = 3,
        };
        HX_STRUCT(SerializedOnly)
        struct SliderEvent : engine::HelixEvent<f32>
        {
            static constexpr StringView kTypeName = "SliderEvent";
        };

        Slider();

        HX_PROPERTY() engine::RectTransform* fillRect() const { return fillRect_.Get(); }
        void SetFillRect(engine::RectTransform* value);
        HX_PROPERTY() engine::RectTransform* handleRect() const { return handleRect_.Get(); }
        void SetHandleRect(engine::RectTransform* value);
        HX_PROPERTY() Direction direction() const { return direction_; }
        void SetDirection(Direction value);
        HX_PROPERTY() f32 minValue() const { return minValue_; }
        void SetMinValue(f32 value);
        HX_PROPERTY() f32 maxValue() const { return maxValue_; }
        void SetMaxValue(f32 value);
        HX_PROPERTY() bool wholeNumbers() const { return wholeNumbers_; }
        void SetWholeNumbers(bool value);
        HX_PROPERTY() virtual f32 value() const;
        virtual void SetValue(f32 value) { Set(value, true); }
        HX_FUNCTION() virtual void SetValueWithoutNotify(f32 input) { Set(input, false); }
        HX_PROPERTY() f32 normalizedValue() const;
        void SetNormalizedValue(f32 value);
        HX_EVENT(Internal) engine::Event<f32>& onValueChangedNative() { return onValueChanged_.listeners(); }
        SliderEvent& onValueChanged() { return onValueChanged_; }

        HX_FUNCTION() virtual void Rebuild(CanvasUpdate executing) override { (void)executing; }
        HX_FUNCTION() virtual void LayoutComplete() override {}
        HX_FUNCTION() virtual void GraphicUpdateComplete() override {}
        engine::Transform* transform() const override { return engine::Component::transform(); }
        bool IsDestroyed() const override { return UIBehaviour::IsDestroyed(); }
        engine::Object* ElementObject() override { return this; }

        HX_FUNCTION() void OnPointerDown(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnDrag(PointerEventData* eventData) override;
        HX_FUNCTION() void OnMove(AxisEventData* eventData) override;
        HX_FUNCTION() Selectable* FindSelectableOnLeft() override;
        HX_FUNCTION() Selectable* FindSelectableOnRight() override;
        HX_FUNCTION() Selectable* FindSelectableOnUp() override;
        HX_FUNCTION() Selectable* FindSelectableOnDown() override;
        HX_FUNCTION() virtual void OnInitializePotentialDrag(PointerEventData* eventData) override;
        // The direction, the RectTransform's layout flipped to it when asked.
        HX_FUNCTION() void SetDirection(Direction direction, bool includeRectLayouts);

        HX_FUNCTION(Protected) virtual void Set(f32 input, bool sendCallback = true);
        HX_FUNCTION(Protected) virtual void Update();

        void OnEnable() override;
        void OnDisable() override;
        void OnDidApplyAnimationProperties() override;
        void OnRectTransformDimensionsChange() override;

    protected:
        bool HandlesMore(EventKind kind) const override { return kind == EventKind::Drag || kind == EventKind::InitializePotentialDrag; }
        void HandleMore(EventKind kind, BaseEventData* data) override;

    private:
        i32 axis() const { return direction_ == Direction::LeftToRight || direction_ == Direction::RightToLeft ? 0 : 1; }
        bool reverseValue() const { return direction_ == Direction::RightToLeft || direction_ == Direction::TopToBottom; }
        f32 stepSize() const { return wholeNumbers_ ? 1.0f : (maxValue_ - minValue_) * 0.1f; }
        void UpdateCachedReferences();
        f32 ClampValue(f32 input) const;
        void UpdateVisuals();
        void UpdateDrag(PointerEventData* eventData, engine::Camera* camera);
        bool MayDrag(PointerEventData* eventData) const;

        HX_FIELD(SerializeField, Name = "m_FillRect") engine::PPtr<engine::RectTransform> fillRect_;
        HX_FIELD(SerializeField, Name = "m_HandleRect") engine::PPtr<engine::RectTransform> handleRect_;
        HX_FIELD(SerializeField, Name = "m_Direction") Direction direction_ = Direction::LeftToRight;
        HX_FIELD(SerializeField, Name = "m_MinValue") f32 minValue_ = 0;
        HX_FIELD(SerializeField, Name = "m_MaxValue") f32 maxValue_ = 1;
        HX_FIELD(SerializeField, Name = "m_WholeNumbers") bool wholeNumbers_ = false;
        HX_FIELD(SerializeField, Name = "m_Value") f32 value_ = 0;
        HX_FIELD(SerializeField, Name = "m_OnValueChanged") SliderEvent onValueChanged_;
        engine::PPtr<engine::Object> fillImage_;
        engine::PPtr<engine::RectTransform> fillContainerRect_, handleContainerRect_;
        Vector2 offset_ = Vector2(0, 0);
        engine::DrivenRectTransformTracker tracker_;
        bool delayedUpdateVisuals_ = false;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::RectTransform")
    class Scrollbar : public Selectable, public IBeginDragHandler, public IDragHandler, public IInitializePotentialDragHandler, public ICanvasElement
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        HX_ENUM()
        enum class Direction : i32
        {
            LeftToRight = 0,
            RightToLeft = 1,
            BottomToTop = 2,
            TopToBottom = 3,
        };
        HX_STRUCT(SerializedOnly)
        struct ScrollEvent : engine::HelixEvent<f32>
        {
            static constexpr StringView kTypeName = "ScrollEvent";
        };

        Scrollbar();

        HX_PROPERTY() engine::RectTransform* handleRect() const { return handleRect_.Get(); }
        void SetHandleRect(engine::RectTransform* value);
        HX_PROPERTY() Direction direction() const { return direction_; }
        void SetDirection(Direction value);
        // The value in steps (when it has steps).
        HX_PROPERTY() f32 value() const;
        void SetValue(f32 value) { Set(value, true); }
        HX_FUNCTION() virtual void SetValueWithoutNotify(f32 input) { Set(input, false); }
        HX_PROPERTY() f32 size() const { return size_; }
        void SetSize(f32 value);
        HX_PROPERTY() i32 numberOfSteps() const { return numberOfSteps_; }
        void SetNumberOfSteps(i32 value);
        HX_EVENT(Internal) engine::Event<f32>& onValueChangedNative() { return onValueChanged_.listeners(); }
        ScrollEvent& onValueChanged() { return onValueChanged_; }

        HX_FUNCTION() virtual void Rebuild(CanvasUpdate executing) override { (void)executing; }
        HX_FUNCTION() virtual void LayoutComplete() override {}
        HX_FUNCTION() virtual void GraphicUpdateComplete() override {}
        engine::Transform* transform() const override { return engine::Component::transform(); }
        bool IsDestroyed() const override { return UIBehaviour::IsDestroyed(); }
        engine::Object* ElementObject() override { return this; }

        HX_FUNCTION() virtual void OnBeginDrag(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnDrag(PointerEventData* eventData) override;
        HX_FUNCTION() void OnPointerDown(PointerEventData* eventData) override;
        HX_FUNCTION() void OnPointerUp(PointerEventData* eventData) override;
        HX_FUNCTION() void OnMove(AxisEventData* eventData) override;
        HX_FUNCTION() Selectable* FindSelectableOnLeft() override;
        HX_FUNCTION() Selectable* FindSelectableOnRight() override;
        HX_FUNCTION() Selectable* FindSelectableOnUp() override;
        HX_FUNCTION() Selectable* FindSelectableOnDown() override;
        HX_FUNCTION() virtual void OnInitializePotentialDrag(PointerEventData* eventData) override;
        HX_FUNCTION() void SetDirection(Direction direction, bool includeRectLayouts);

        HX_FUNCTION(Protected) virtual void Update();

        void OnEnable() override;
        void OnDisable() override;
        void OnDidApplyAnimationProperties() override;
        void OnRectTransformDimensionsChange() override;

        // Stepped value of the raw one (for the scroll views, which set it unclamped).
        void Set(f32 input, bool sendCallback = true);

    protected:
        bool HandlesMore(EventKind kind) const override
        {
            return kind == EventKind::BeginDrag || kind == EventKind::Drag || kind == EventKind::InitializePotentialDrag;
        }
        void HandleMore(EventKind kind, BaseEventData* data) override;

    private:
        i32 axis() const { return direction_ == Direction::LeftToRight || direction_ == Direction::RightToLeft ? 0 : 1; }
        bool reverseValue() const { return direction_ == Direction::RightToLeft || direction_ == Direction::TopToBottom; }
        f32 stepSize() const { return numberOfSteps_ > 1 ? 1.0f / f32(numberOfSteps_ - 1) : 0.1f; }
        void UpdateCachedReferences();
        void UpdateVisuals();
        void UpdateDrag(PointerEventData* eventData);
        void UpdateDrag(engine::RectTransform* containerRect, Vector2 position, engine::Camera* camera);
        void DoUpdateDrag(Vector2 handleCorner, f32 remainingSize);
        bool MayDrag(PointerEventData* eventData) const;
        // While pressed beside the handle (not dragging): a page toward the point at every frame's end.
        engine::Coroutine ClickRepeat(Vector2 screenPosition, engine::Camera* camera);

        HX_FIELD(SerializeField, Name = "m_HandleRect") engine::PPtr<engine::RectTransform> handleRect_;
        HX_FIELD(SerializeField, Name = "m_Direction") Direction direction_ = Direction::LeftToRight;
        HX_FIELD(SerializeField, Name = "m_Value") f32 value_ = 0;
        HX_FIELD(SerializeField, Name = "m_Size") f32 size_ = 0.2f;
        HX_FIELD(SerializeField, Name = "m_NumberOfSteps") i32 numberOfSteps_ = 0;
        HX_FIELD(SerializeField, Name = "m_OnValueChanged") ScrollEvent onValueChanged_;
        engine::PPtr<engine::RectTransform> containerRect_;
        Vector2 offset_ = Vector2(0, 0);
        engine::DrivenRectTransformTracker tracker_;
        engine::CoroutineRef pointerDownRepeat_;
        bool isPointerDownAndNotDragging_ = false, delayedUpdateVisuals_ = false;
    };
}
