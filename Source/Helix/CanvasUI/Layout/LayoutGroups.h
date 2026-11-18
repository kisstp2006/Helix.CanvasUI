#pragma once

#include <Helix/CanvasUI/Layout/LayoutRebuilder.h>
#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Transform/RectTransform.h>
#include <Helix/UI/Engine/GuiTypes.h>

// The layout components: groups place their children (in a row, a column or a grid) from their sizes, LayoutElement
// gives an element's sizes, ContentSizeFitter sizes its own RectTransform to its content, AspectRatioFitter keeps an
// aspect, CanvasScaler sets its canvas's scale factor (constant, by the screen against a reference resolution, or by
// physical size).
namespace helix::canvasui
{
    // A group's padding as files hold it (the RectOffset object the group owns).
    HX_STRUCT(SerializedOnly)
    struct PaddingField
    {
        static constexpr StringView kTypeName = "RectOffset";
        ui::gui::RectOffset* offset = nullptr;

        template <class V>
        void Transfer(V& v)
        {
            i32 l = offset->left(), r = offset->right(), t = offset->top(), b = offset->bottom();
            v.Field("m_Left", l);
            v.Field("m_Right", r);
            v.Field("m_Top", t);
            v.Field("m_Bottom", b);
            offset->SetLeft(l), offset->SetRight(r), offset->SetTop(t), offset->SetBottom(b);
        }
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Abstract, RequireComponent = "engine::RectTransform")
    class LayoutGroup : public UIBehaviour, public ILayoutElement, public ILayoutGroup
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        LayoutGroup();
        ~LayoutGroup() override;

        HX_PROPERTY() ui::gui::RectOffset* padding() const { return padding_.offset; }
        void SetPadding(ui::gui::RectOffset* value);
        HX_PROPERTY() ui::gui::TextAnchor childAlignment() const { return childAlignment_; }
        void SetChildAlignment(ui::gui::TextAnchor value);
        HX_PROPERTY(ReadOnly, Protected) engine::RectTransform* rectTransform() const;
        HX_PROPERTY(ReadOnly, Protected, Name = "rectChildren") Array<engine::RectTransform*> rectChildrenInternal() const;

        // ILayoutElement
        HX_FUNCTION() void CalculateLayoutInputHorizontal() override;
        HX_FUNCTION() void CalculateLayoutInputVertical() override = 0;
        HX_PROPERTY() f32 minWidth() const override { return totalMin_.x; }
        HX_PROPERTY() f32 preferredWidth() const override { return totalPreferred_.x; }
        HX_PROPERTY() f32 flexibleWidth() const override { return totalFlexible_.x; }
        HX_PROPERTY() f32 minHeight() const override { return totalMin_.y; }
        HX_PROPERTY() f32 preferredHeight() const override { return totalPreferred_.y; }
        HX_PROPERTY() f32 flexibleHeight() const override { return totalFlexible_.y; }
        HX_PROPERTY() i32 layoutPriority() const override { return 0; }
        HX_FUNCTION() void SetLayoutHorizontal() override = 0;
        HX_FUNCTION() void SetLayoutVertical() override = 0;

        HX_FUNCTION(Protected) f32 GetTotalMinSize(i32 axis) const { return axis == 0 ? totalMin_.x : totalMin_.y; }
        HX_FUNCTION(Protected) f32 GetTotalPreferredSize(i32 axis) const { return axis == 0 ? totalPreferred_.x : totalPreferred_.y; }
        HX_FUNCTION(Protected) f32 GetTotalFlexibleSize(i32 axis) const { return axis == 0 ? totalFlexible_.x : totalFlexible_.y; }
        // Where the content of the required size starts on the axis (the padding and the alignment's share of the room left).
        HX_FUNCTION(Protected) f32 GetStartOffset(i32 axis, f32 requiredSpaceWithoutPadding) const;
        HX_FUNCTION(Protected) f32 GetAlignmentOnAxis(i32 axis) const;
        HX_FUNCTION(Protected) void SetLayoutInputForAxis(f32 totalMin, f32 totalPreferred, f32 totalFlexible, i32 axis);
        HX_FUNCTION(Protected) void SetChildAlongAxis(engine::RectTransform* rect, i32 axis, f32 pos);
        HX_FUNCTION(Protected) void SetChildAlongAxis(engine::RectTransform* rect, i32 axis, f32 pos, f32 size);
        HX_FUNCTION(Protected) void SetChildAlongAxisWithScale(engine::RectTransform* rect, i32 axis, f32 pos, f32 scaleFactor);
        HX_FUNCTION(Protected) void SetChildAlongAxisWithScale(engine::RectTransform* rect, i32 axis, f32 pos, f32 size, f32 scaleFactor);
        HX_FUNCTION(Protected) void SetDirty();

        ILayoutElement* AsLayoutElement() override { return this; }
        ILayoutController* AsLayoutController() override { return static_cast<ILayoutGroup*>(this); }
        void OnEnable() override;
        void OnDisable() override;
        void OnDidApplyAnimationProperties() override { SetDirty(); }
        void OnRectTransformDimensionsChange() override;
        HX_FUNCTION(Protected) virtual void OnTransformChildrenChanged() { SetDirty(); }

        // Marks again after the layout loop (a change made while it runs).
        static void RunDelayed();

    protected:
        Array<engine::PPtr<engine::RectTransform>> rectChildren_;
        engine::DrivenRectTransformTracker tracker_;

    private:
        HX_FIELD(SerializeField, Name = "m_Padding") PaddingField padding_;
        HX_FIELD(SerializeField, Name = "m_ChildAlignment") ui::gui::TextAnchor childAlignment_ = ui::gui::TextAnchor::UpperLeft;
        Vector2 totalMin_ = Vector2(0, 0), totalPreferred_ = Vector2(0, 0), totalFlexible_ = Vector2(0, 0);
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Abstract)
    class HorizontalOrVerticalLayoutGroup : public LayoutGroup
    {
    public:
        HX_PROPERTY() f32 spacing() const { return spacing_; }
        void SetSpacing(f32 value);
        HX_PROPERTY() bool childForceExpandWidth() const { return childForceExpandWidth_; }
        void SetChildForceExpandWidth(bool value);
        HX_PROPERTY() bool childForceExpandHeight() const { return childForceExpandHeight_; }
        void SetChildForceExpandHeight(bool value);
        HX_PROPERTY() bool childControlWidth() const { return childControlWidth_; }
        void SetChildControlWidth(bool value);
        HX_PROPERTY() bool childControlHeight() const { return childControlHeight_; }
        void SetChildControlHeight(bool value);
        HX_PROPERTY() bool childScaleWidth() const { return childScaleWidth_; }
        void SetChildScaleWidth(bool value);
        HX_PROPERTY() bool childScaleHeight() const { return childScaleHeight_; }
        void SetChildScaleHeight(bool value);
        HX_PROPERTY() bool reverseArrangement() const { return reverseArrangement_; }
        void SetReverseArrangement(bool value);

        // The sizes along the axis (summed along the group's direction, the largest across it).
        HX_FUNCTION(Protected) void CalcAlongAxis(i32 axis, bool isVertical);
        HX_FUNCTION(Protected) void SetChildrenAlongAxis(i32 axis, bool isVertical);
        HX_FUNCTION(Protected) virtual void Update() {}
        void Reset() override;

    private:
        void ChildSizes(engine::RectTransform* child, i32 axis, bool controlSize, bool forceExpand, f32& min, f32& preferred, f32& flexible) const;
        void Set(bool& field, bool value);

        HX_FIELD(SerializeField, Name = "m_Spacing") f32 spacing_ = 0;
        HX_FIELD(SerializeField, Name = "m_ChildForceExpandWidth") bool childForceExpandWidth_ = true;
        HX_FIELD(SerializeField, Name = "m_ChildForceExpandHeight") bool childForceExpandHeight_ = true;
        HX_FIELD(SerializeField, Name = "m_ChildControlWidth") bool childControlWidth_ = true;
        HX_FIELD(SerializeField, Name = "m_ChildControlHeight") bool childControlHeight_ = true;
        HX_FIELD(SerializeField, Name = "m_ChildScaleWidth") bool childScaleWidth_ = false;
        HX_FIELD(SerializeField, Name = "m_ChildScaleHeight") bool childScaleHeight_ = false;
        HX_FIELD(SerializeField, Name = "m_ReverseArrangement") bool reverseArrangement_ = false;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class HorizontalLayoutGroup : public HorizontalOrVerticalLayoutGroup
    {
    public:
        HX_FUNCTION() void CalculateLayoutInputHorizontal() override;
        HX_FUNCTION() void CalculateLayoutInputVertical() override { CalcAlongAxis(1, false); }
        HX_FUNCTION() void SetLayoutHorizontal() override { SetChildrenAlongAxis(0, false); }
        HX_FUNCTION() void SetLayoutVertical() override { SetChildrenAlongAxis(1, false); }
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class VerticalLayoutGroup : public HorizontalOrVerticalLayoutGroup
    {
    public:
        HX_FUNCTION() void CalculateLayoutInputHorizontal() override;
        HX_FUNCTION() void CalculateLayoutInputVertical() override { CalcAlongAxis(1, true); }
        HX_FUNCTION() void SetLayoutHorizontal() override { SetChildrenAlongAxis(0, true); }
        HX_FUNCTION() void SetLayoutVertical() override { SetChildrenAlongAxis(1, true); }
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class GridLayoutGroup : public LayoutGroup
    {
    public:
        HX_ENUM()
        enum class Corner : i32
        {
            UpperLeft = 0,
            UpperRight = 1,
            LowerLeft = 2,
            LowerRight = 3,
        };
        HX_ENUM()
        enum class Axis : i32
        {
            Horizontal = 0,
            Vertical = 1,
        };
        HX_ENUM()
        enum class Constraint : i32
        {
            Flexible = 0,
            FixedColumnCount = 1,
            FixedRowCount = 2,
        };

        HX_PROPERTY() Corner startCorner() const { return startCorner_; }
        void SetStartCorner(Corner value);
        HX_PROPERTY() Axis startAxis() const { return startAxis_; }
        void SetStartAxis(Axis value);
        HX_PROPERTY() Vector2 cellSize() const { return cellSize_; }
        void SetCellSize(Vector2 value);
        HX_PROPERTY() Vector2 spacing() const { return spacing_; }
        void SetSpacing(Vector2 value);
        HX_PROPERTY() Constraint constraint() const { return constraint_; }
        void SetConstraint(Constraint value);
        // At least 1.
        HX_PROPERTY() i32 constraintCount() const { return constraintCount_; }
        void SetConstraintCount(i32 value);

        HX_FUNCTION() void CalculateLayoutInputHorizontal() override;
        HX_FUNCTION() void CalculateLayoutInputVertical() override;
        HX_FUNCTION() void SetLayoutHorizontal() override { SetCellsAlongAxis(0); }
        HX_FUNCTION() void SetLayoutVertical() override { SetCellsAlongAxis(1); }

    private:
        void SetCellsAlongAxis(i32 axis);

        HX_FIELD(SerializeField, Name = "m_StartCorner") Corner startCorner_ = Corner::UpperLeft;
        HX_FIELD(SerializeField, Name = "m_StartAxis") Axis startAxis_ = Axis::Horizontal;
        HX_FIELD(SerializeField, Name = "m_CellSize") Vector2 cellSize_ = Vector2(100, 100);
        HX_FIELD(SerializeField, Name = "m_Spacing") Vector2 spacing_ = Vector2(0, 0);
        HX_FIELD(SerializeField, Name = "m_Constraint") Constraint constraint_ = Constraint::Flexible;
        HX_FIELD(SerializeField, Name = "m_ConstraintCount") i32 constraintCount_ = 2;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::RectTransform")
    class LayoutElement : public UIBehaviour, public ILayoutElement, public ILayoutIgnorer
    {
    public:
        HX_PROPERTY() bool ignoreLayout() const override { return ignoreLayout_; }
        virtual void SetIgnoreLayout(bool value);
        HX_PROPERTY() f32 minWidth() const override { return minWidth_; }
        virtual void SetMinWidth(f32 value);
        HX_PROPERTY() f32 minHeight() const override { return minHeight_; }
        virtual void SetMinHeight(f32 value);
        HX_PROPERTY() f32 preferredWidth() const override { return preferredWidth_; }
        virtual void SetPreferredWidth(f32 value);
        HX_PROPERTY() f32 preferredHeight() const override { return preferredHeight_; }
        virtual void SetPreferredHeight(f32 value);
        HX_PROPERTY() f32 flexibleWidth() const override { return flexibleWidth_; }
        virtual void SetFlexibleWidth(f32 value);
        HX_PROPERTY() f32 flexibleHeight() const override { return flexibleHeight_; }
        virtual void SetFlexibleHeight(f32 value);
        HX_PROPERTY() i32 layoutPriority() const override { return layoutPriority_; }
        virtual void SetLayoutPriority(i32 value);
        HX_FUNCTION() void CalculateLayoutInputHorizontal() override {}
        HX_FUNCTION() void CalculateLayoutInputVertical() override {}
        HX_FUNCTION(Protected) void SetDirty();

        ILayoutElement* AsLayoutElement() override { return this; }
        ILayoutIgnorer* AsLayoutIgnorer() override { return this; }
        void OnEnable() override { SetDirty(); }
        void OnDisable() override { SetDirty(); }
        void OnTransformParentChanged() override { SetDirty(); }
        void OnBeforeTransformParentChanged() override { SetDirty(); }
        void OnDidApplyAnimationProperties() override { SetDirty(); }

    private:
        HX_FIELD(SerializeField, Name = "m_IgnoreLayout") bool ignoreLayout_ = false;
        HX_FIELD(SerializeField, Name = "m_MinWidth") f32 minWidth_ = -1;
        HX_FIELD(SerializeField, Name = "m_MinHeight") f32 minHeight_ = -1;
        HX_FIELD(SerializeField, Name = "m_PreferredWidth") f32 preferredWidth_ = -1;
        HX_FIELD(SerializeField, Name = "m_PreferredHeight") f32 preferredHeight_ = -1;
        HX_FIELD(SerializeField, Name = "m_FlexibleWidth") f32 flexibleWidth_ = -1;
        HX_FIELD(SerializeField, Name = "m_FlexibleHeight") f32 flexibleHeight_ = -1;
        HX_FIELD(SerializeField, Name = "m_LayoutPriority") i32 layoutPriority_ = 1;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::RectTransform")
    class ContentSizeFitter : public UIBehaviour, public ILayoutSelfController
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        HX_ENUM()
        enum class FitMode : i32
        {
            Unconstrained = 0,
            MinSize = 1,
            PreferredSize = 2,
        };

        HX_PROPERTY() FitMode horizontalFit() const { return horizontalFit_; }
        void SetHorizontalFit(FitMode value);
        HX_PROPERTY() FitMode verticalFit() const { return verticalFit_; }
        void SetVerticalFit(FitMode value);
        HX_FUNCTION() void SetLayoutHorizontal() override;
        HX_FUNCTION() void SetLayoutVertical() override;
        HX_FUNCTION(Protected) void SetDirty();

        ILayoutController* AsLayoutController() override { return this; }
        void OnEnable() override;
        void OnDisable() override;
        void OnRectTransformDimensionsChange() override { SetDirty(); }

    private:
        void Fit(i32 axis);

        HX_FIELD(SerializeField, Name = "m_HorizontalFit") FitMode horizontalFit_ = FitMode::Unconstrained;
        HX_FIELD(SerializeField, Name = "m_VerticalFit") FitMode verticalFit_ = FitMode::Unconstrained;
        engine::DrivenRectTransformTracker tracker_;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::RectTransform")
    class AspectRatioFitter : public UIBehaviour, public ILayoutSelfController
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        HX_ENUM()
        enum class AspectMode : i32
        {
            None = 0,
            WidthControlsHeight = 1,
            HeightControlsWidth = 2,
            FitInParent = 3,
            EnvelopeParent = 4,
        };

        HX_PROPERTY() AspectMode aspectMode() const { return aspectMode_; }
        void SetAspectMode(AspectMode value);
        HX_PROPERTY() f32 aspectRatio() const { return aspectRatio_; }
        void SetAspectRatio(f32 value);
        HX_FUNCTION() void SetLayoutHorizontal() override {}
        HX_FUNCTION() void SetLayoutVertical() override {}
        HX_FUNCTION(Protected) void SetDirty() { UpdateRect(); }
        // Not on a root canvas of a screen space (whose RectTransform the canvas drives).
        HX_FUNCTION() bool IsComponentValidOnObject() const;
        // FitInParent and EnvelopeParent need a parent.
        HX_FUNCTION() bool IsAspectModeValid() const;
        HX_FUNCTION(Protected) virtual void Update();

        ILayoutController* AsLayoutController() override { return this; }
        void OnEnable() override;
        void Start() override;
        void OnDisable() override;
        void OnTransformParentChanged() override;
        void OnRectTransformDimensionsChange() override { UpdateRect(); }

    private:
        void UpdateRect();
        Vector2 ParentSize() const;

        HX_FIELD(SerializeField, Name = "m_AspectMode") AspectMode aspectMode_ = AspectMode::None;
        HX_FIELD(SerializeField, Name = "m_AspectRatio") f32 aspectRatio_ = 1;
        engine::DrivenRectTransformTracker tracker_;
        bool delayedSetDirty_ = false;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::Canvas")
    class CanvasScaler : public UIBehaviour
    {
    public:
        HX_ENUM()
        enum class ScaleMode : i32
        {
            ConstantPixelSize = 0,
            ScaleWithScreenSize = 1,
            ConstantPhysicalSize = 2,
        };
        HX_ENUM()
        enum class ScreenMatchMode : i32
        {
            MatchWidthOrHeight = 0,
            Expand = 1,
            Shrink = 2,
        };
        HX_ENUM()
        enum class Unit : i32
        {
            Centimeters = 0,
            Millimeters = 1,
            Inches = 2,
            Points = 3,
            Picas = 4,
        };

        ~CanvasScaler() override;

        HX_PROPERTY() ScaleMode uiScaleMode() const { return uiScaleMode_; }
        void SetUiScaleMode(ScaleMode value) { uiScaleMode_ = value; }
        HX_PROPERTY() f32 referencePixelsPerUnit() const { return referencePixelsPerUnit_; }
        void SetReferencePixelsPerUnitProperty(f32 value) { referencePixelsPerUnit_ = value; }
        // At least 0.01.
        HX_PROPERTY() f32 scaleFactor() const { return scaleFactor_; }
        void SetScaleFactorProperty(f32 value) { scaleFactor_ = value > 0.01f ? value : 0.01f; }
        HX_PROPERTY() Vector2 referenceResolution() const { return referenceResolution_; }
        void SetReferenceResolution(Vector2 value);
        HX_PROPERTY() ScreenMatchMode screenMatchMode() const { return screenMatchMode_; }
        void SetScreenMatchMode(ScreenMatchMode value) { screenMatchMode_ = value; }
        HX_PROPERTY() f32 matchWidthOrHeight() const { return matchWidthOrHeight_; }
        void SetMatchWidthOrHeight(f32 value) { matchWidthOrHeight_ = value; }
        HX_PROPERTY() Unit physicalUnit() const { return physicalUnit_; }
        void SetPhysicalUnit(Unit value) { physicalUnit_ = value; }
        HX_PROPERTY() f32 fallbackScreenDPI() const { return fallbackScreenDPI_; }
        void SetFallbackScreenDPI(f32 value) { fallbackScreenDPI_ = value; }
        HX_PROPERTY() f32 defaultSpriteDPI() const { return defaultSpriteDPI_; }
        void SetDefaultSpriteDPI(f32 value) { defaultSpriteDPI_ = value > 1 ? value : 1; }
        HX_PROPERTY() f32 dynamicPixelsPerUnit() const { return dynamicPixelsPerUnit_; }
        void SetDynamicPixelsPerUnit(f32 value) { dynamicPixelsPerUnit_ = value; }

        HX_FUNCTION(Protected) virtual void Handle();
        HX_FUNCTION(Protected) virtual void HandleWorldCanvas();
        HX_FUNCTION(Protected) virtual void HandleConstantPixelSize();
        HX_FUNCTION(Protected) virtual void HandleScaleWithScreenSize();
        HX_FUNCTION(Protected) virtual void HandleConstantPhysicalSize();
        HX_FUNCTION(Protected, Name = "SetScaleFactor") void ApplyScaleFactor(f32 scaleFactor);
        HX_FUNCTION(Protected, Name = "SetReferencePixelsPerUnit") void ApplyReferencePixelsPerUnit(f32 referencePixelsPerUnit);

        void OnEnable() override;
        void OnDisable() override;

    private:
        engine::Canvas* CanvasOf() const;

        HX_FIELD(SerializeField, Name = "m_UiScaleMode") ScaleMode uiScaleMode_ = ScaleMode::ConstantPixelSize;
        HX_FIELD(SerializeField, Name = "m_ReferencePixelsPerUnit") f32 referencePixelsPerUnit_ = 100;
        HX_FIELD(SerializeField, Name = "m_ScaleFactor") f32 scaleFactor_ = 1;
        HX_FIELD(SerializeField, Name = "m_ReferenceResolution") Vector2 referenceResolution_ = Vector2(800, 600);
        HX_FIELD(SerializeField, Name = "m_ScreenMatchMode") ScreenMatchMode screenMatchMode_ = ScreenMatchMode::MatchWidthOrHeight;
        HX_FIELD(SerializeField, Name = "m_MatchWidthOrHeight") f32 matchWidthOrHeight_ = 0;
        HX_FIELD(SerializeField, Name = "m_PhysicalUnit") Unit physicalUnit_ = Unit::Points;
        HX_FIELD(SerializeField, Name = "m_FallbackScreenDPI") f32 fallbackScreenDPI_ = 96;
        HX_FIELD(SerializeField, Name = "m_DefaultSpriteDPI") f32 defaultSpriteDPI_ = 96;
        HX_FIELD(SerializeField, Name = "m_DynamicPixelsPerUnit") f32 dynamicPixelsPerUnit_ = 1;
        f32 prevScaleFactor_ = 1, prevReferencePixelsPerUnit_ = 100;
        u32 hook_ = 0;
    };
}
