#include <Helix/CanvasUI/Layout/LayoutGroups.h>

#include <Helix/CanvasUI/Core/Implementation/CanvasUILog.h>
#include <Helix/CanvasUI/Core/ManagedCalls.h>
#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Screen/Screen.h>

#include <cmath>

namespace helix::canvasui
{
    namespace
    {
        f32 Get(Vector2 v, i32 axis) { return axis == 0 ? v.x : v.y; }
        void Put(Vector2& v, i32 axis, f32 x) { (axis == 0 ? v.x : v.y) = x; }
        f32 Get(Vector3 v, i32 axis) { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; }
        f32 Clamp(f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
        f32 Max(f32 a, f32 b) { return a > b ? a : b; }
        f32 Min(f32 a, f32 b) { return a < b ? a : b; }

        engine::RectTransform* AsRect(engine::Transform* t) { return t && t->Is<engine::RectTransform>() ? static_cast<engine::RectTransform*>(t) : nullptr; }

        // The layout groups that asked for a rebuild while the layout ran: marked at the next frame.
        Array<engine::PPtr<engine::RectTransform>>& Delayed()
        {
            static Array<engine::PPtr<engine::RectTransform>>* d = new Array<engine::PPtr<engine::RectTransform>>();
            return *d;
        }
    }

    // ------------------------------------------------------------------------------------------------ LayoutGroup

    LayoutGroup::LayoutGroup() { padding_.offset = new ui::gui::RectOffset(); }

    LayoutGroup::~LayoutGroup()
    {
        // A C# object of the padding lives on as its own (C# owns the objects it holds).
        if (padding_.offset && padding_.offset->Wrapper() == 0)
            delete padding_.offset;
    }

    void LayoutGroup::SetPadding(ui::gui::RectOffset* value)
    {
        if (!value || value == padding_.offset)
            return;
        padding_.offset->SetLeft(value->left()), padding_.offset->SetRight(value->right());
        padding_.offset->SetTop(value->top()), padding_.offset->SetBottom(value->bottom());
        SetDirty();
    }

    void LayoutGroup::SetChildAlignment(ui::gui::TextAnchor value)
    {
        if (childAlignment_ == value)
            return;
        childAlignment_ = value;
        SetDirty();
    }

    engine::RectTransform* LayoutGroup::rectTransform() const { return AsRect(engine::Component::transform()); }

    Array<engine::RectTransform*> LayoutGroup::rectChildrenInternal() const
    {
        Array<engine::RectTransform*> out;
        for (const engine::PPtr<engine::RectTransform>& r : rectChildren_)
            if (engine::RectTransform* x = r.Get())
                out.Push(x);
        return out;
    }

    void LayoutGroup::CalculateLayoutInputHorizontal()
    {
        rectChildren_.Clear();
        engine::RectTransform* rt = rectTransform();
        for (u32 i = 0; rt && i < rt->childCount(); i++)
        {
            engine::RectTransform* child = AsRect(rt->GetChild(i));
            if (!child || !child->gameObject() || !child->gameObject()->activeInHierarchy())
                continue;
            // A child counts unless every layout ignorer on it ignores the layout.
            bool any = false, keep = false;
            engine::GameObject* g = child->gameObject();
            for (u32 k = 0; k < g->GetComponentCount() && !keep; k++)
            {
                engine::Component* c = g->GetComponentAtIndex(k);
                UIBehaviour* b = AsUIBehaviour(c);
                bool ignorer = (b && b->AsLayoutIgnorer()) || (ManagedCalls::IsManaged(c) && ManagedCalls::Implements(c, "ILayoutIgnorer"));
                if (!ignorer)
                    continue;
                any = true;
                keep = !layout::IgnoresLayout(c);
            }
            if (!any || keep)
                rectChildren_.Push(engine::PPtr<engine::RectTransform>(child));
        }
        tracker_.Clear();
    }

    f32 LayoutGroup::GetStartOffset(i32 axis, f32 requiredSpaceWithoutPadding) const
    {
        ui::gui::RectOffset* p = padding_.offset;
        f32 required = requiredSpaceWithoutPadding + f32(axis == 0 ? p->horizontal() : p->vertical());
        engine::RectTransform* rt = rectTransform();
        f32 available = rt ? (axis == 0 ? rt->rect().width : rt->rect().height) : 0;
        f32 surplus = available - required;
        return f32(axis == 0 ? p->left() : p->top()) + surplus * GetAlignmentOnAxis(axis);
    }

    f32 LayoutGroup::GetAlignmentOnAxis(i32 axis) const
    {
        i32 a = i32(childAlignment_);
        return axis == 0 ? f32(a % 3) * 0.5f : f32(a / 3) * 0.5f;
    }

    void LayoutGroup::SetLayoutInputForAxis(f32 totalMin, f32 totalPreferred, f32 totalFlexible, i32 axis)
    {
        Put(totalMin_, axis, totalMin);
        Put(totalPreferred_, axis, totalPreferred);
        Put(totalFlexible_, axis, totalFlexible);
    }

    void LayoutGroup::SetChildAlongAxis(engine::RectTransform* rect, i32 axis, f32 pos) { SetChildAlongAxisWithScale(rect, axis, pos, 1.0f); }

    void LayoutGroup::SetChildAlongAxis(engine::RectTransform* rect, i32 axis, f32 pos, f32 size) { SetChildAlongAxisWithScale(rect, axis, pos, size, 1.0f); }

    void LayoutGroup::SetChildAlongAxisWithScale(engine::RectTransform* rect, i32 axis, f32 pos, f32 scaleFactor)
    {
        if (!rect)
            return;
        using P = engine::DrivenTransformProperties;
        tracker_.Add(this, rect, P(i32(P::Anchors) | i32(axis == 0 ? P::AnchoredPositionX : P::AnchoredPositionY)));
        // From the top left corner; the size kept, the position by the scaled size.
        rect->SetAnchorMin(Vector2(0, 1));
        rect->SetAnchorMax(Vector2(0, 1));
        Vector2 anchored = rect->anchoredPosition();
        f32 size = Get(rect->sizeDelta(), axis), pivot = Get(rect->pivot(), axis);
        Put(anchored, axis, axis == 0 ? pos + size * pivot * scaleFactor : -pos - size * (1 - pivot) * scaleFactor);
        rect->SetAnchoredPosition(anchored);
    }

    void LayoutGroup::SetChildAlongAxisWithScale(engine::RectTransform* rect, i32 axis, f32 pos, f32 size, f32 scaleFactor)
    {
        if (!rect)
            return;
        using P = engine::DrivenTransformProperties;
        tracker_.Add(this, rect, P(i32(P::Anchors) | i32(axis == 0 ? P::AnchoredPositionX : P::AnchoredPositionY) | i32(axis == 0 ? P::SizeDeltaX : P::SizeDeltaY)));
        rect->SetAnchorMin(Vector2(0, 1));
        rect->SetAnchorMax(Vector2(0, 1));
        Vector2 sizeDelta = rect->sizeDelta();
        Put(sizeDelta, axis, size);
        rect->SetSizeDelta(sizeDelta);
        Vector2 anchored = rect->anchoredPosition();
        f32 pivot = Get(rect->pivot(), axis);
        Put(anchored, axis, axis == 0 ? pos + size * pivot * scaleFactor : -pos - size * (1 - pivot) * scaleFactor);
        rect->SetAnchoredPosition(anchored);
    }

    void LayoutGroup::SetDirty()
    {
        if (!IsActive())
            return;
        engine::RectTransform* rt = rectTransform();
        if (!CanvasUpdateRegistry::IsRebuildingLayout())
            LayoutRebuilder::MarkLayoutForRebuild(rt);
        else
        {
            // Not while the layout runs: at the next frame (as the reference's coroutine waits a frame).
            Delayed().Push(engine::PPtr<engine::RectTransform>(rt));
            static bool hooked = false;
            if (!hooked)
            {
                hooked = true;
                engine::Canvas::preWillRenderCanvasesInternal().Add([](void*) { LayoutGroup::RunDelayed(); });
            }
        }
    }

    void LayoutGroup::RunDelayed()
    {
        Array<engine::PPtr<engine::RectTransform>> list = Delayed();
        Delayed().Clear();
        for (const engine::PPtr<engine::RectTransform>& r : list)
            if (engine::RectTransform* rt = r.Get())
                LayoutRebuilder::MarkLayoutForRebuild(rt);
    }

    void LayoutGroup::OnEnable()
    {
        UIBehaviour::OnEnable();
        SetDirty();
    }

    void LayoutGroup::OnDisable()
    {
        tracker_.Clear();
        LayoutRebuilder::MarkLayoutForRebuild(rectTransform());
        UIBehaviour::OnDisable();
    }

    void LayoutGroup::OnRectTransformDimensionsChange()
    {
        UIBehaviour::OnRectTransformDimensionsChange();
        // Only a root group (no group above it) starts a rebuild of its own.
        engine::Transform* parent = engine::Component::transform()->parent();
        bool root = true;
        if (parent && parent->gameObject())
        {
            engine::GameObject* g = parent->gameObject();
            for (u32 i = 0; i < g->GetComponentCount() && root; i++)
                root = !layout::IsGroup(g->GetComponentAtIndex(i));
        }
        if (root)
            SetDirty();
    }

    // ------------------------------------------------------------------------------ HorizontalOrVerticalLayoutGroup

    void HorizontalOrVerticalLayoutGroup::Set(bool& field, bool value)
    {
        if (field == value)
            return;
        field = value;
        SetDirty();
    }

    void HorizontalOrVerticalLayoutGroup::SetSpacing(f32 value)
    {
        if (spacing_ == value)
            return;
        spacing_ = value;
        SetDirty();
    }

    void HorizontalOrVerticalLayoutGroup::SetChildForceExpandWidth(bool value) { Set(childForceExpandWidth_, value); }
    void HorizontalOrVerticalLayoutGroup::SetChildForceExpandHeight(bool value) { Set(childForceExpandHeight_, value); }
    void HorizontalOrVerticalLayoutGroup::SetChildControlWidth(bool value) { Set(childControlWidth_, value); }
    void HorizontalOrVerticalLayoutGroup::SetChildControlHeight(bool value) { Set(childControlHeight_, value); }
    void HorizontalOrVerticalLayoutGroup::SetChildScaleWidth(bool value) { Set(childScaleWidth_, value); }
    void HorizontalOrVerticalLayoutGroup::SetChildScaleHeight(bool value) { Set(childScaleHeight_, value); }
    void HorizontalOrVerticalLayoutGroup::SetReverseArrangement(bool value) { Set(reverseArrangement_, value); }

    void HorizontalOrVerticalLayoutGroup::Reset()
    {
        LayoutGroup::Reset();
        // New components leave their children's sizes alone until asked.
        childControlWidth_ = false;
        childControlHeight_ = false;
    }

    void HorizontalOrVerticalLayoutGroup::ChildSizes(engine::RectTransform* child, i32 axis, bool controlSize, bool forceExpand, f32& min, f32& preferred,
                                                     f32& flexible) const
    {
        if (!controlSize)
        {
            min = Get(child->sizeDelta(), axis);
            preferred = min;
            flexible = 0;
        }
        else
        {
            min = LayoutUtility::GetMinSize(child, axis);
            preferred = LayoutUtility::GetPreferredSize(child, axis);
            flexible = LayoutUtility::GetFlexibleSize(child, axis);
        }
        if (forceExpand)
            flexible = Max(flexible, 1);
    }

    void HorizontalOrVerticalLayoutGroup::CalcAlongAxis(i32 axis, bool isVertical)
    {
        ui::gui::RectOffset* p = padding();
        f32 combinedPadding = f32(axis == 0 ? p->horizontal() : p->vertical());
        bool control = axis == 0 ? childControlWidth_ : childControlHeight_;
        bool scale = axis == 0 ? childScaleWidth_ : childScaleHeight_;
        bool expand = axis == 0 ? childForceExpandWidth_ : childForceExpandHeight_;
        f32 totalMin = combinedPadding, totalPreferred = combinedPadding, totalFlexible = 0;
        bool across = isVertical ^ (axis == 1);
        Array<engine::RectTransform*> children = rectChildrenInternal();
        for (engine::RectTransform* child : children)
        {
            f32 min, preferred, flexible;
            ChildSizes(child, axis, control, expand, min, preferred, flexible);
            if (scale)
            {
                f32 s = Get(child->localScale(), axis);
                min *= s, preferred *= s, flexible *= s;
            }
            if (across)
            {
                totalMin = Max(min + combinedPadding, totalMin);
                totalPreferred = Max(preferred + combinedPadding, totalPreferred);
                totalFlexible = Max(flexible, totalFlexible);
            }
            else
            {
                totalMin += min + spacing_;
                totalPreferred += preferred + spacing_;
                totalFlexible += flexible;
            }
        }
        if (!across && !children.Empty())
        {
            totalMin -= spacing_;
            totalPreferred -= spacing_;
        }
        totalPreferred = Max(totalMin, totalPreferred);
        SetLayoutInputForAxis(totalMin, totalPreferred, totalFlexible, axis);
    }

    void HorizontalOrVerticalLayoutGroup::SetChildrenAlongAxis(i32 axis, bool isVertical)
    {
        engine::RectTransform* rt = rectTransform();
        f32 size = rt ? (axis == 0 ? rt->rect().width : rt->rect().height) : 0;
        bool control = axis == 0 ? childControlWidth_ : childControlHeight_;
        bool scale = axis == 0 ? childScaleWidth_ : childScaleHeight_;
        bool expand = axis == 0 ? childForceExpandWidth_ : childForceExpandHeight_;
        f32 alignment = GetAlignmentOnAxis(axis);
        bool across = isVertical ^ (axis == 1);
        ui::gui::RectOffset* p = padding();
        Array<engine::RectTransform*> children = rectChildrenInternal();
        i32 count = i32(children.Size());
        auto at = [&](i32 k) { return children[usize(reverseArrangement_ ? count - 1 - k : k)]; };
        if (across)
        {
            f32 inner = size - f32(axis == 0 ? p->horizontal() : p->vertical());
            for (i32 k = 0; k < count; k++)
            {
                engine::RectTransform* child = at(k);
                f32 min, preferred, flexible;
                ChildSizes(child, axis, control, expand, min, preferred, flexible);
                f32 s = scale ? Get(child->localScale(), axis) : 1.0f;
                f32 required = Clamp(inner, min, flexible > 0 ? size : preferred);
                f32 start = GetStartOffset(axis, required * s);
                if (control)
                    SetChildAlongAxisWithScale(child, axis, start, required, s);
                else
                {
                    f32 offset = (required - Get(child->sizeDelta(), axis)) * alignment;
                    SetChildAlongAxisWithScale(child, axis, start + offset, s);
                }
            }
            return;
        }
        f32 pos = f32(axis == 0 ? p->left() : p->top());
        f32 flexibleMultiplier = 0;
        f32 surplus = size - GetTotalPreferredSize(axis);
        if (surplus > 0)
        {
            if (GetTotalFlexibleSize(axis) == 0)
                pos = GetStartOffset(axis, GetTotalPreferredSize(axis) - f32(axis == 0 ? p->horizontal() : p->vertical()));
            else if (GetTotalFlexibleSize(axis) > 0)
                flexibleMultiplier = surplus / GetTotalFlexibleSize(axis);
        }
        f32 minMaxLerp = 0;
        if (GetTotalMinSize(axis) != GetTotalPreferredSize(axis))
            minMaxLerp = Clamp((size - GetTotalMinSize(axis)) / (GetTotalPreferredSize(axis) - GetTotalMinSize(axis)), 0, 1);
        for (i32 k = 0; k < count; k++)
        {
            engine::RectTransform* child = at(k);
            f32 min, preferred, flexible;
            ChildSizes(child, axis, control, expand, min, preferred, flexible);
            f32 s = scale ? Get(child->localScale(), axis) : 1.0f;
            f32 childSize = min + (preferred - min) * minMaxLerp + flexible * flexibleMultiplier;
            if (control)
                SetChildAlongAxisWithScale(child, axis, pos, childSize, s);
            else
            {
                f32 offset = (childSize - Get(child->sizeDelta(), axis)) * alignment;
                SetChildAlongAxisWithScale(child, axis, pos + offset, s);
            }
            pos += childSize * s + spacing_;
        }
    }

    void HorizontalLayoutGroup::CalculateLayoutInputHorizontal()
    {
        LayoutGroup::CalculateLayoutInputHorizontal();
        CalcAlongAxis(0, false);
    }

    void VerticalLayoutGroup::CalculateLayoutInputHorizontal()
    {
        LayoutGroup::CalculateLayoutInputHorizontal();
        CalcAlongAxis(0, true);
    }

    // -------------------------------------------------------------------------------------------- GridLayoutGroup

    void GridLayoutGroup::SetStartCorner(Corner value)
    {
        if (startCorner_ != value)
            startCorner_ = value, SetDirty();
    }

    void GridLayoutGroup::SetStartAxis(Axis value)
    {
        if (startAxis_ != value)
            startAxis_ = value, SetDirty();
    }

    void GridLayoutGroup::SetCellSize(Vector2 value)
    {
        if (!(cellSize_ == value))
            cellSize_ = value, SetDirty();
    }

    void GridLayoutGroup::SetSpacing(Vector2 value)
    {
        if (!(spacing_ == value))
            spacing_ = value, SetDirty();
    }

    void GridLayoutGroup::SetConstraint(Constraint value)
    {
        if (constraint_ != value)
            constraint_ = value, SetDirty();
    }

    void GridLayoutGroup::SetConstraintCount(i32 value)
    {
        value = value > 1 ? value : 1;
        if (constraintCount_ != value)
            constraintCount_ = value, SetDirty();
    }

    void GridLayoutGroup::CalculateLayoutInputHorizontal()
    {
        LayoutGroup::CalculateLayoutInputHorizontal();
        i32 n = i32(rectChildren_.Size());
        i32 minColumns = 0, preferredColumns = 0;
        if (constraint_ == Constraint::FixedColumnCount)
            minColumns = preferredColumns = constraintCount_;
        else if (constraint_ == Constraint::FixedRowCount)
            minColumns = preferredColumns = i32(std::ceil(f32(n) / f32(constraintCount_) - 0.001f));
        else
        {
            minColumns = 1;
            preferredColumns = i32(std::ceil(std::sqrt(f32(n))));
        }
        f32 h = f32(padding()->horizontal());
        SetLayoutInputForAxis(h + (cellSize_.x + spacing_.x) * f32(minColumns) - spacing_.x, h + (cellSize_.x + spacing_.x) * f32(preferredColumns) - spacing_.x, -1, 0);
    }

    void GridLayoutGroup::CalculateLayoutInputVertical()
    {
        i32 n = i32(rectChildren_.Size());
        i32 minRows = 0;
        if (constraint_ == Constraint::FixedColumnCount)
            minRows = i32(std::ceil(f32(n) / f32(constraintCount_) - 0.001f));
        else if (constraint_ == Constraint::FixedRowCount)
            minRows = constraintCount_;
        else
        {
            f32 width = rectTransform()->rect().width;
            i32 cellsX = i32(std::floor((width - f32(padding()->horizontal()) + spacing_.x + 0.001f) / (cellSize_.x + spacing_.x)));
            cellsX = cellsX > 1 ? cellsX : 1;
            minRows = i32(std::ceil(f32(n) / f32(cellsX)));
        }
        f32 minSpace = f32(padding()->vertical()) + (cellSize_.y + spacing_.y) * f32(minRows) - spacing_.y;
        SetLayoutInputForAxis(minSpace, minSpace, -1, 1);
    }

    void GridLayoutGroup::SetCellsAlongAxis(i32 axis)
    {
        using P = engine::DrivenTransformProperties;
        Array<engine::RectTransform*> children = rectChildrenInternal();
        i32 n = i32(children.Size());
        if (axis == 0)
        {
            // The sizes on the horizontal pass, the positions (both axes) on the vertical one.
            for (engine::RectTransform* rect : children)
            {
                tracker_.Add(this, rect, P(i32(P::Anchors) | i32(P::AnchoredPosition) | i32(P::SizeDelta)));
                rect->SetAnchorMin(Vector2(0, 1));
                rect->SetAnchorMax(Vector2(0, 1));
                rect->SetSizeDelta(cellSize_);
            }
            return;
        }
        Rect r = rectTransform()->rect();
        ui::gui::RectOffset* p = padding();
        i32 cellsX = 1, cellsY = 1;
        if (constraint_ == Constraint::FixedColumnCount)
        {
            cellsX = constraintCount_;
            if (n > cellsX)
                cellsY = n / cellsX + (n % cellsX > 0 ? 1 : 0);
        }
        else if (constraint_ == Constraint::FixedRowCount)
        {
            cellsY = constraintCount_;
            if (n > cellsY)
                cellsX = n / cellsY + (n % cellsY > 0 ? 1 : 0);
        }
        else
        {
            constexpr i32 kMax = 0x7fffffff;
            cellsX = cellSize_.x + spacing_.x <= 0 ? kMax : i32(Max(1, std::floor((r.width - f32(p->horizontal()) + spacing_.x + 0.001f) / (cellSize_.x + spacing_.x))));
            cellsY = cellSize_.y + spacing_.y <= 0 ? kMax : i32(Max(1, std::floor((r.height - f32(p->vertical()) + spacing_.y + 0.001f) / (cellSize_.y + spacing_.y))));
        }
        i32 cornerX = i32(startCorner_) % 2, cornerY = i32(startCorner_) / 2;
        i32 perMainAxis, actualX, actualY;
        auto clampi = [](i32 v, i32 lo, i32 hi) { return v < lo ? lo : (v > hi ? hi : v); };
        if (startAxis_ == Axis::Horizontal)
        {
            perMainAxis = cellsX;
            actualX = clampi(cellsX, 1, n);
            actualY = constraint_ == Constraint::FixedRowCount ? (cellsY < n ? cellsY : n) : clampi(cellsY, 1, i32(std::ceil(f32(n) / f32(perMainAxis))));
        }
        else
        {
            perMainAxis = cellsY;
            actualY = clampi(cellsY, 1, n);
            actualX = constraint_ == Constraint::FixedColumnCount ? (cellsX < n ? cellsX : n) : clampi(cellsX, 1, i32(std::ceil(f32(n) / f32(perMainAxis))));
        }
        Vector2 required(f32(actualX) * cellSize_.x + f32(actualX - 1) * spacing_.x, f32(actualY) * cellSize_.y + f32(actualY - 1) * spacing_.y);
        Vector2 start(GetStartOffset(0, required.x), GetStartOffset(1, required.y));
        for (i32 i = 0; i < n; i++)
        {
            i32 x = startAxis_ == Axis::Horizontal ? i % perMainAxis : i / perMainAxis;
            i32 y = startAxis_ == Axis::Horizontal ? i / perMainAxis : i % perMainAxis;
            if (cornerX == 1)
                x = actualX - 1 - x;
            if (cornerY == 1)
                y = actualY - 1 - y;
            SetChildAlongAxis(children[usize(i)], 0, start.x + (cellSize_.x + spacing_.x) * f32(x), cellSize_.x);
            SetChildAlongAxis(children[usize(i)], 1, start.y + (cellSize_.y + spacing_.y) * f32(y), cellSize_.y);
        }
    }

    // ---------------------------------------------------------------------------------------------- LayoutElement

    void LayoutElement::SetDirty()
    {
        if (IsActive())
            LayoutRebuilder::MarkLayoutForRebuild(AsRect(engine::Component::transform()));
    }

    void LayoutElement::SetIgnoreLayout(bool value)
    {
        if (ignoreLayout_ != value)
            ignoreLayout_ = value, SetDirty();
    }

#define HX_LAYOUT_SETTER(Name, field, Type)  \
    void LayoutElement::Set##Name(Type value) \
    {                                        \
        if (field != value)                  \
            field = value, SetDirty();       \
    }
    HX_LAYOUT_SETTER(MinWidth, minWidth_, f32)
    HX_LAYOUT_SETTER(MinHeight, minHeight_, f32)
    HX_LAYOUT_SETTER(PreferredWidth, preferredWidth_, f32)
    HX_LAYOUT_SETTER(PreferredHeight, preferredHeight_, f32)
    HX_LAYOUT_SETTER(FlexibleWidth, flexibleWidth_, f32)
    HX_LAYOUT_SETTER(FlexibleHeight, flexibleHeight_, f32)
    HX_LAYOUT_SETTER(LayoutPriority, layoutPriority_, i32)
#undef HX_LAYOUT_SETTER

    // ------------------------------------------------------------------------------------------ ContentSizeFitter

    void ContentSizeFitter::SetHorizontalFit(FitMode value)
    {
        if (horizontalFit_ != value)
            horizontalFit_ = value, SetDirty();
    }

    void ContentSizeFitter::SetVerticalFit(FitMode value)
    {
        if (verticalFit_ != value)
            verticalFit_ = value, SetDirty();
    }

    void ContentSizeFitter::SetDirty()
    {
        if (IsActive())
            LayoutRebuilder::MarkLayoutForRebuild(AsRect(engine::Component::transform()));
    }

    void ContentSizeFitter::Fit(i32 axis)
    {
        engine::RectTransform* rt = AsRect(engine::Component::transform());
        FitMode fit = axis == 0 ? horizontalFit_ : verticalFit_;
        using P = engine::DrivenTransformProperties;
        if (fit == FitMode::Unconstrained)
        {
            // Tracked, nothing driven.
            tracker_.Add(this, rt, P::None);
            return;
        }
        tracker_.Add(this, rt, axis == 0 ? P::SizeDeltaX : P::SizeDeltaY);
        f32 size = fit == FitMode::MinSize ? LayoutUtility::GetMinSize(rt, axis) : LayoutUtility::GetPreferredSize(rt, axis);
        rt->SetSizeWithCurrentAnchors(engine::RectTransform::Axis(axis), size);
    }

    void ContentSizeFitter::SetLayoutHorizontal()
    {
        tracker_.Clear();
        Fit(0);
    }

    void ContentSizeFitter::SetLayoutVertical() { Fit(1); }

    void ContentSizeFitter::OnEnable()
    {
        UIBehaviour::OnEnable();
        SetDirty();
    }

    void ContentSizeFitter::OnDisable()
    {
        tracker_.Clear();
        LayoutRebuilder::MarkLayoutForRebuild(AsRect(engine::Component::transform()));
        UIBehaviour::OnDisable();
    }

    // ------------------------------------------------------------------------------------------ AspectRatioFitter

    void AspectRatioFitter::SetAspectMode(AspectMode value)
    {
        if (aspectMode_ != value)
            aspectMode_ = value, SetDirty();
    }

    void AspectRatioFitter::SetAspectRatio(f32 value)
    {
        if (aspectRatio_ != value)
            aspectRatio_ = value, SetDirty();
    }

    bool AspectRatioFitter::IsComponentValidOnObject() const
    {
        engine::Canvas* c = ComponentOf<engine::Canvas>(gameObject());
        return !(c && c->isRootCanvas() && c->renderMode() != engine::RenderMode::WorldSpace);
    }

    bool AspectRatioFitter::IsAspectModeValid() const
    {
        bool parent = engine::Component::transform()->parent() != nullptr;
        return parent || (aspectMode_ != AspectMode::EnvelopeParent && aspectMode_ != AspectMode::FitInParent);
    }

    void AspectRatioFitter::Update()
    {
        if (delayedSetDirty_)
        {
            delayedSetDirty_ = false;
            SetDirty();
        }
    }

    Vector2 AspectRatioFitter::ParentSize() const
    {
        engine::RectTransform* parent = AsRect(engine::Component::transform()->parent());
        return parent ? Vector2(parent->rect().width, parent->rect().height) : Vector2(0, 0);
    }

    void AspectRatioFitter::UpdateRect()
    {
        if (!IsActive() || !IsComponentValidOnObject())
            return;
        engine::RectTransform* rt = AsRect(engine::Component::transform());
        tracker_.Clear();
        using P = engine::DrivenTransformProperties;
        switch (aspectMode_)
        {
        case AspectMode::None: break;
        case AspectMode::HeightControlsWidth:
            tracker_.Add(this, rt, P::SizeDeltaX);
            rt->SetSizeWithCurrentAnchors(engine::RectTransform::Axis::Horizontal, rt->rect().height * aspectRatio_);
            break;
        case AspectMode::WidthControlsHeight:
            tracker_.Add(this, rt, P::SizeDeltaY);
            rt->SetSizeWithCurrentAnchors(engine::RectTransform::Axis::Vertical, rt->rect().width / aspectRatio_);
            break;
        case AspectMode::FitInParent:
        case AspectMode::EnvelopeParent:
        {
            if (!engine::Component::transform()->parent())
                break;
            tracker_.Add(this, rt, P(i32(P::Anchors) | i32(P::AnchoredPosition) | i32(P::SizeDeltaX) | i32(P::SizeDeltaY)));
            rt->SetAnchorMin(Vector2(0, 0));
            rt->SetAnchorMax(Vector2(1, 1));
            rt->SetAnchoredPosition(Vector2(0, 0));
            Vector2 parent = ParentSize(), sizeDelta(0, 0);
            // Fit: the side that runs out first sets the size; envelope: the other one.
            auto deltaFor = [&](f32 size, i32 axis) { return size - Get(parent, axis) * (Get(rt->anchorMax(), axis) - Get(rt->anchorMin(), axis)); };
            if ((parent.y * aspectRatio_ < parent.x) ^ (aspectMode_ == AspectMode::FitInParent))
                sizeDelta.y = deltaFor(parent.x / aspectRatio_, 1);
            else
                sizeDelta.x = deltaFor(parent.y * aspectRatio_, 0);
            rt->SetSizeDelta(sizeDelta);
            break;
        }
        }
    }

    void AspectRatioFitter::OnEnable()
    {
        UIBehaviour::OnEnable();
        SetDirty();
    }

    void AspectRatioFitter::Start()
    {
        // Not valid here: it turns itself off (the reference warns in its editor).
        if (!IsComponentValidOnObject() || !IsAspectModeValid())
            SetEnabled(false);
    }

    void AspectRatioFitter::OnDisable()
    {
        tracker_.Clear();
        LayoutRebuilder::MarkLayoutForRebuild(AsRect(engine::Component::transform()));
        UIBehaviour::OnDisable();
    }

    void AspectRatioFitter::OnTransformParentChanged()
    {
        UIBehaviour::OnTransformParentChanged();
        SetDirty();
    }

    // ----------------------------------------------------------------------------------------------- CanvasScaler

    CanvasScaler::~CanvasScaler()
    {
        if (hook_)
            engine::Canvas::preWillRenderCanvasesInternal().Remove(hook_);
    }

    engine::Canvas* CanvasScaler::CanvasOf() const { return ComponentOf<engine::Canvas>(gameObject()); }

    void CanvasScaler::SetReferenceResolution(Vector2 value)
    {
        constexpr f32 kMin = 0.00001f;
        if (value.x > -kMin && value.x < kMin)
        {
            HX_LOG_WARNING(CanvasUILog, "referenceResolution.x must be greater than 0: set to {}.", kMin);
            value.x = kMin * (value.x < 0 ? -1.0f : 1.0f);
        }
        if (value.y > -kMin && value.y < kMin)
        {
            HX_LOG_WARNING(CanvasUILog, "referenceResolution.y must be greater than 0: set to {}.", kMin);
            value.y = kMin * (value.y < 0 ? -1.0f : 1.0f);
        }
        referenceResolution_ = value;
    }

    void CanvasScaler::OnEnable()
    {
        UIBehaviour::OnEnable();
        Handle();
        if (!hook_)
            hook_ = engine::Canvas::preWillRenderCanvasesInternal().Add([](void* self) { static_cast<CanvasScaler*>(self)->Handle(); }, this);
    }

    void CanvasScaler::OnDisable()
    {
        ApplyScaleFactor(1);
        ApplyReferencePixelsPerUnit(100);
        if (hook_)
            engine::Canvas::preWillRenderCanvasesInternal().Remove(hook_);
        hook_ = 0;
        UIBehaviour::OnDisable();
    }

    void CanvasScaler::Handle()
    {
        engine::Canvas* c = CanvasOf();
        if (!c || !c->isRootCanvas())
            return;
        if (c->renderMode() == engine::RenderMode::WorldSpace)
        {
            HandleWorldCanvas();
            return;
        }
        switch (uiScaleMode_)
        {
        case ScaleMode::ConstantPixelSize: HandleConstantPixelSize(); break;
        case ScaleMode::ScaleWithScreenSize: HandleScaleWithScreenSize(); break;
        case ScaleMode::ConstantPhysicalSize: HandleConstantPhysicalSize(); break;
        }
    }

    void CanvasScaler::HandleWorldCanvas()
    {
        ApplyScaleFactor(dynamicPixelsPerUnit_);
        ApplyReferencePixelsPerUnit(referencePixelsPerUnit_);
    }

    void CanvasScaler::HandleConstantPixelSize()
    {
        ApplyScaleFactor(scaleFactor_);
        ApplyReferencePixelsPerUnit(referencePixelsPerUnit_);
    }

    void CanvasScaler::HandleScaleWithScreenSize()
    {
        Vector2 screen = CanvasOf()->renderingDisplaySize();
        f32 scale = 0;
        switch (screenMatchMode_)
        {
        case ScreenMatchMode::MatchWidthOrHeight:
        {
            // Averaged in log space: twice as wide and half as high evens out at 0.5.
            f32 w = std::log2(screen.x / referenceResolution_.x), h = std::log2(screen.y / referenceResolution_.y);
            scale = std::exp2(w + (h - w) * matchWidthOrHeight_);
            break;
        }
        case ScreenMatchMode::Expand: scale = Min(screen.x / referenceResolution_.x, screen.y / referenceResolution_.y); break;
        case ScreenMatchMode::Shrink: scale = Max(screen.x / referenceResolution_.x, screen.y / referenceResolution_.y); break;
        }
        ApplyScaleFactor(scale);
        ApplyReferencePixelsPerUnit(referencePixelsPerUnit_);
    }

    void CanvasScaler::HandleConstantPhysicalSize()
    {
        f32 current = engine::Screen::dpi();
        f32 dpi = current == 0 ? fallbackScreenDPI_ : current;
        f32 target = 1;
        switch (physicalUnit_)
        {
        case Unit::Centimeters: target = 2.54f; break;
        case Unit::Millimeters: target = 25.4f; break;
        case Unit::Inches: target = 1; break;
        case Unit::Points: target = 72; break;
        case Unit::Picas: target = 6; break;
        }
        ApplyScaleFactor(dpi / target);
        ApplyReferencePixelsPerUnit(referencePixelsPerUnit_ * target / defaultSpriteDPI_);
    }

    void CanvasScaler::ApplyScaleFactor(f32 scaleFactor)
    {
        if (scaleFactor == prevScaleFactor_)
            return;
        if (engine::Canvas* c = CanvasOf())
            c->SetScaleFactor(scaleFactor);
        prevScaleFactor_ = scaleFactor;
    }

    void CanvasScaler::ApplyReferencePixelsPerUnit(f32 referencePixelsPerUnit)
    {
        if (referencePixelsPerUnit == prevReferencePixelsPerUnit_)
            return;
        if (engine::Canvas* c = CanvasOf())
            c->SetReferencePixelsPerUnit(referencePixelsPerUnit);
        prevReferencePixelsPerUnit_ = referencePixelsPerUnit;
    }

    void LayoutGroup::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        tracker_ = engine::DrivenRectTransformTracker();
        rectChildren_.Clear();
        // Its own padding object with the original's values.
        ui::gui::RectOffset* from = padding_.offset;
        padding_.offset = from ? new ui::gui::RectOffset(from->left(), from->right(), from->top(), from->bottom()) : new ui::gui::RectOffset();
    }

    void ContentSizeFitter::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        tracker_ = engine::DrivenRectTransformTracker();
    }

    void AspectRatioFitter::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        tracker_ = engine::DrivenRectTransformTracker();
    }
}
