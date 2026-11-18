#pragma once

#include <Helix/CanvasUI/Controls/Sliders.h>
#include <Helix/CanvasUI/Layout/LayoutRebuilder.h>
#include <Helix/Engine/Transform/RectTransform.h>

// A scroll view: its content moved inside its viewport by drags, the wheel and its scrollbars; the movement
// unrestricted, elastic (springing back past the ends) or clamped, with inertia; the scrollbars shown always or only
// when needed (and the viewport grown into their room when hidden).
namespace helix::canvasui
{
    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::RectTransform")
    class ScrollRect : public UIBehaviour,
                       public INativeEventHandler,
                       public IInitializePotentialDragHandler,
                       public IBeginDragHandler,
                       public IEndDragHandler,
                       public IDragHandler,
                       public IScrollHandler,
                       public ICanvasElement,
                       public ILayoutElement,
                       public ILayoutGroup
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        HX_ENUM()
        enum class MovementType : i32
        {
            Unrestricted = 0,
            Elastic = 1,
            Clamped = 2,
        };
        HX_ENUM()
        enum class ScrollbarVisibility : i32
        {
            Permanent = 0,
            AutoHide = 1,
            AutoHideAndExpandViewport = 2,
        };
        HX_STRUCT(SerializedOnly)
        struct ScrollRectEvent : engine::HelixEvent<Vector2>
        {
            static constexpr StringView kTypeName = "ScrollRectEvent";
        };

        ScrollRect();

        HX_PROPERTY() engine::RectTransform* content() const { return content_.Get(); }
        void SetContent(engine::RectTransform* value) { content_ = engine::PPtr<engine::RectTransform>(value); }
        HX_PROPERTY() bool horizontal() const { return horizontal_; }
        void SetHorizontal(bool value) { horizontal_ = value; }
        HX_PROPERTY() bool vertical() const { return vertical_; }
        void SetVertical(bool value) { vertical_ = value; }
        HX_PROPERTY() MovementType movementType() const { return movementType_; }
        void SetMovementType(MovementType value) { movementType_ = value; }
        HX_PROPERTY() f32 elasticity() const { return elasticity_; }
        void SetElasticity(f32 value) { elasticity_ = value; }
        HX_PROPERTY() bool inertia() const { return inertia_; }
        void SetInertia(bool value) { inertia_ = value; }
        HX_PROPERTY() f32 decelerationRate() const { return decelerationRate_; }
        void SetDecelerationRate(f32 value) { decelerationRate_ = value; }
        HX_PROPERTY() f32 scrollSensitivity() const { return scrollSensitivity_; }
        void SetScrollSensitivity(f32 value) { scrollSensitivity_ = value; }
        HX_PROPERTY() engine::RectTransform* viewport() const { return viewport_.Get(); }
        void SetViewport(engine::RectTransform* value);
        HX_PROPERTY() Scrollbar* horizontalScrollbar() const;
        void SetHorizontalScrollbar(Scrollbar* value);
        HX_PROPERTY() Scrollbar* verticalScrollbar() const;
        void SetVerticalScrollbar(Scrollbar* value);
        HX_PROPERTY() ScrollbarVisibility horizontalScrollbarVisibility() const { return horizontalScrollbarVisibility_; }
        void SetHorizontalScrollbarVisibility(ScrollbarVisibility value);
        HX_PROPERTY() ScrollbarVisibility verticalScrollbarVisibility() const { return verticalScrollbarVisibility_; }
        void SetVerticalScrollbarVisibility(ScrollbarVisibility value);
        HX_PROPERTY() f32 horizontalScrollbarSpacing() const { return horizontalScrollbarSpacing_; }
        void SetHorizontalScrollbarSpacing(f32 value);
        HX_PROPERTY() f32 verticalScrollbarSpacing() const { return verticalScrollbarSpacing_; }
        void SetVerticalScrollbarSpacing(f32 value);
        HX_EVENT(Internal) engine::Event<Vector2>& onValueChangedNative() { return onValueChanged_.listeners(); }
        ScrollRectEvent& onValueChanged() { return onValueChanged_; }
        HX_PROPERTY() Vector2 velocity() const { return velocity_; }
        void SetVelocity(Vector2 value) { velocity_ = value; }
        HX_PROPERTY() Vector2 normalizedPosition() { return Vector2(horizontalNormalizedPosition(), verticalNormalizedPosition()); }
        void SetNormalizedPosition(Vector2 value);
        HX_PROPERTY() f32 horizontalNormalizedPosition();
        void SetHorizontalNormalizedPosition(f32 value) { SetNormalizedPosition(value, 0); }
        HX_PROPERTY() f32 verticalNormalizedPosition();
        void SetVerticalNormalizedPosition(f32 value) { SetNormalizedPosition(value, 1); }
        HX_PROPERTY(ReadOnly, Protected) engine::RectTransform* viewRect() const;
        // The reference's protected fields, read by subclasses.
        HX_PROPERTY(ReadOnly, Protected, Name = "m_ContentBounds") Bounds ContentBoundsOf() const { return contentBounds_; }
        HX_PROPERTY(ReadOnly, Protected, Name = "m_ViewBounds") Bounds ViewBoundsOf() const { return viewBounds_; }
        HX_PROPERTY(ReadOnly, Protected, Name = "m_ContentStartPosition") Vector2 ContentStartPositionOf() const { return contentStartPosition_; }

        HX_FUNCTION() virtual void Rebuild(CanvasUpdate executing) override;
        HX_FUNCTION() virtual void LayoutComplete() override {}
        HX_FUNCTION() virtual void GraphicUpdateComplete() override {}
        engine::Transform* transform() const override { return engine::Component::transform(); }
        bool IsDestroyed() const override { return UIBehaviour::IsDestroyed(); }
        engine::Object* ElementObject() override { return this; }

        HX_FUNCTION() bool IsActive() const override;
        HX_FUNCTION() virtual void StopMovement() { velocity_ = Vector2(0, 0); }
        HX_FUNCTION() virtual void OnScroll(PointerEventData* data) override;
        HX_FUNCTION() virtual void OnInitializePotentialDrag(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnBeginDrag(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnEndDrag(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnDrag(PointerEventData* eventData) override;

        // ILayoutElement and ILayoutGroup: no sizes of its own; the viewport and the scrollbars laid out.
        HX_FUNCTION() virtual void CalculateLayoutInputHorizontal() override {}
        HX_FUNCTION() virtual void CalculateLayoutInputVertical() override {}
        HX_PROPERTY() virtual f32 minWidth() const override { return -1; }
        HX_PROPERTY() virtual f32 preferredWidth() const override { return -1; }
        HX_PROPERTY() virtual f32 flexibleWidth() const override { return -1; }
        HX_PROPERTY() virtual f32 minHeight() const override { return -1; }
        HX_PROPERTY() virtual f32 preferredHeight() const override { return -1; }
        HX_PROPERTY() virtual f32 flexibleHeight() const override { return -1; }
        HX_PROPERTY() virtual i32 layoutPriority() const override { return -1; }
        HX_FUNCTION() virtual void SetLayoutHorizontal() override;
        HX_FUNCTION() virtual void SetLayoutVertical() override;

        HX_FUNCTION(Protected) virtual void SetContentAnchoredPosition(Vector2 position);
        HX_FUNCTION(Protected) virtual void LateUpdate();
        HX_FUNCTION(Protected) void UpdatePrevData();
        HX_FUNCTION(Protected) virtual void SetNormalizedPosition(f32 value, i32 axis);
        HX_FUNCTION(Protected) void UpdateBounds();
        HX_FUNCTION(Protected) void SetDirty();
        HX_FUNCTION(Protected) void SetDirtyCaching();

        void OnEnable() override;
        void OnDisable() override;
        void OnRectTransformDimensionsChange() override { SetDirty(); }

        bool Handles(EventKind kind) const override;
        void HandleEvent(EventKind kind, BaseEventData* data) override;
        INativeEventHandler* AsEventHandler() override { return this; }
        ILayoutElement* AsLayoutElement() override { return this; }
        ILayoutController* AsLayoutController() override { return static_cast<ILayoutGroup*>(this); }

        // The content made at least the view's size (grown from its pivot); the static helpers of the reference.
        static void AdjustBounds(Bounds& viewBounds, Vector2& contentPivot, Vector3& contentSize, Vector3& contentPos);
        static Bounds InternalGetBounds(const Vector3 (&corners)[4], const Matrix4x4& viewWorldToLocalMatrix);
        static Vector2 InternalCalculateOffset(const Bounds& viewBounds, const Bounds& contentBounds, bool horizontal, bool vertical, MovementType movementType,
                                               Vector2 delta);
        static f32 RubberDelta(f32 overStretching, f32 viewSize);

    private:
        engine::RectTransform* rectTransform() const;
        void UpdateCachedData();
        void EnsureLayoutHasRebuilt();
        void UpdateScrollbars(Vector2 offset);
        void UpdateScrollbarVisibility();
        void UpdateScrollbarLayout();
        bool hScrollingNeeded() const;
        bool vScrollingNeeded() const;
        Bounds GetBounds() const;
        Vector2 CalculateOffset(Vector2 delta) const;
        void Listen(Scrollbar* bar, i32 axis, bool on);
        static void OnHorizontalBarValue(void* user, f32 value);
        static void OnVerticalBarValue(void* user, f32 value);

        HX_FIELD(SerializeField, Name = "m_Content") engine::PPtr<engine::RectTransform> content_;
        HX_FIELD(SerializeField, Name = "m_Horizontal") bool horizontal_ = true;
        HX_FIELD(SerializeField, Name = "m_Vertical") bool vertical_ = true;
        HX_FIELD(SerializeField, Name = "m_MovementType") MovementType movementType_ = MovementType::Elastic;
        HX_FIELD(SerializeField, Name = "m_Elasticity") f32 elasticity_ = 0.1f;
        HX_FIELD(SerializeField, Name = "m_Inertia") bool inertia_ = true;
        HX_FIELD(SerializeField, Name = "m_DecelerationRate") f32 decelerationRate_ = 0.135f;
        HX_FIELD(SerializeField, Name = "m_ScrollSensitivity") f32 scrollSensitivity_ = 1;
        HX_FIELD(SerializeField, Name = "m_Viewport") engine::PPtr<engine::RectTransform> viewport_;
        HX_FIELD(SerializeField, Name = "m_HorizontalScrollbar") engine::PPtr<engine::Object> horizontalScrollbar_;
        HX_FIELD(SerializeField, Name = "m_VerticalScrollbar") engine::PPtr<engine::Object> verticalScrollbar_;
        HX_FIELD(SerializeField, Name = "m_HorizontalScrollbarVisibility") ScrollbarVisibility horizontalScrollbarVisibility_ = ScrollbarVisibility::Permanent;
        HX_FIELD(SerializeField, Name = "m_VerticalScrollbarVisibility") ScrollbarVisibility verticalScrollbarVisibility_ = ScrollbarVisibility::Permanent;
        HX_FIELD(SerializeField, Name = "m_HorizontalScrollbarSpacing") f32 horizontalScrollbarSpacing_ = 0;
        HX_FIELD(SerializeField, Name = "m_VerticalScrollbarSpacing") f32 verticalScrollbarSpacing_ = 0;
        HX_FIELD(SerializeField, Name = "m_OnValueChanged") ScrollRectEvent onValueChanged_;

        Vector2 pointerStartLocalCursor_ = Vector2(0, 0), contentStartPosition_ = Vector2(0, 0);
        mutable engine::PPtr<engine::RectTransform> viewRect_;
        Bounds contentBounds_, viewBounds_, prevContentBounds_, prevViewBounds_;
        Vector2 velocity_ = Vector2(0, 0), prevPosition_ = Vector2(0, 0);
        bool dragging_ = false, scrolling_ = false, hasRebuiltLayout_ = false;
        bool hSliderExpand_ = false, vSliderExpand_ = false;
        f32 hSliderHeight_ = 0, vSliderWidth_ = 0;
        engine::PPtr<engine::RectTransform> horizontalScrollbarRect_, verticalScrollbarRect_;
        engine::DrivenRectTransformTracker tracker_;
        // The scrollbars this listens to (removed from the same ones).
        engine::PPtr<engine::Object> listenedH_, listenedV_;
    };
}
