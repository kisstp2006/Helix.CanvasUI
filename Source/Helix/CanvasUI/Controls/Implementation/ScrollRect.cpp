#include <Helix/CanvasUI/Controls/ScrollRect.h>

#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Engine/Transform/RectTransformUtility.h>
#include <Helix/Math/Mathf.h>

#include <cmath>
#include <limits>

namespace helix::canvasui
{
    namespace
    {
        using P = engine::DrivenTransformProperties;

        P Props(std::initializer_list<P> list)
        {
            i32 v = 0;
            for (P p : list)
                v |= i32(p);
            return P(v);
        }

        engine::RectTransform* RectOf(engine::Transform* t) { return t && t->Is<engine::RectTransform>() ? static_cast<engine::RectTransform*>(t) : nullptr; }

        Bounds BoundsOfRect(engine::RectTransform* r)
        {
            Rect rect = r->rect();
            Vector2 c = rect.center(), s = rect.size();
            return Bounds(Vector3(c.x, c.y, 0), Vector3(s.x, s.y, 0));
        }

        bool SameBounds(const Bounds& a, const Bounds& b) { return a == b; }
    }

    ScrollRect::ScrollRect()
    {
        static bool registered = false;
        if (!registered)
        {
            registered = true;
            engine::NativeEventEntry e;
            e.storage = [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<ScrollRect*>(o)->onValueChanged_; };
            e.raise = [](engine::Object* o, Span<const scripting::Variant> a) {
                Vector2 v(0, 0);
                if (!a.Empty())
                    a[0].Get(v);
                static_cast<ScrollRect*>(o)->onValueChanged_.Invoke(v);
            };
            engine::RegisterNativeEvent(scripting::TypeOf<ScrollRect>(), 0, e);
        }
    }

    engine::RectTransform* ScrollRect::rectTransform() const { return RectOf(engine::Component::transform()); }

    engine::RectTransform* ScrollRect::viewRect() const
    {
        if (!viewRect_.Get())
            viewRect_ = engine::PPtr<engine::RectTransform>(viewport_.Get() ? viewport_.Get() : rectTransform());
        return viewRect_.Get();
    }

    void ScrollRect::SetViewport(engine::RectTransform* value)
    {
        viewport_ = engine::PPtr<engine::RectTransform>(value);
        SetDirtyCaching();
    }

    Scrollbar* ScrollRect::horizontalScrollbar() const { return static_cast<Scrollbar*>(horizontalScrollbar_.Get()); }
    Scrollbar* ScrollRect::verticalScrollbar() const { return static_cast<Scrollbar*>(verticalScrollbar_.Get()); }

    void ScrollRect::OnHorizontalBarValue(void* user, f32 value) { static_cast<ScrollRect*>(user)->SetHorizontalNormalizedPosition(value); }
    void ScrollRect::OnVerticalBarValue(void* user, f32 value) { static_cast<ScrollRect*>(user)->SetVerticalNormalizedPosition(value); }

    void ScrollRect::Listen(Scrollbar* bar, i32 axis, bool on)
    {
        engine::PPtr<engine::Object>& listened = axis == 0 ? listenedH_ : listenedV_;
        auto callback = axis == 0 ? &ScrollRect::OnHorizontalBarValue : &ScrollRect::OnVerticalBarValue;
        if (auto* old = static_cast<Scrollbar*>(listened.Get()))
            old->onValueChanged().listeners().Remove(callback, this);
        listened = engine::PPtr<engine::Object>();
        if (on && bar)
        {
            bar->onValueChanged().listeners().Add(callback, this);
            listened = engine::PPtr<engine::Object>(bar);
        }
    }

    void ScrollRect::SetHorizontalScrollbar(Scrollbar* value)
    {
        horizontalScrollbar_ = engine::PPtr<engine::Object>(value);
        Listen(value, 0, horizontal_ && value && IsActive());
        SetDirtyCaching();
    }

    void ScrollRect::SetVerticalScrollbar(Scrollbar* value)
    {
        verticalScrollbar_ = engine::PPtr<engine::Object>(value);
        Listen(value, 1, vertical_ && value && IsActive());
        SetDirtyCaching();
    }

    void ScrollRect::SetHorizontalScrollbarVisibility(ScrollbarVisibility value)
    {
        horizontalScrollbarVisibility_ = value;
        SetDirtyCaching();
    }

    void ScrollRect::SetVerticalScrollbarVisibility(ScrollbarVisibility value)
    {
        verticalScrollbarVisibility_ = value;
        SetDirtyCaching();
    }

    void ScrollRect::SetHorizontalScrollbarSpacing(f32 value)
    {
        horizontalScrollbarSpacing_ = value;
        SetDirty();
    }

    void ScrollRect::SetVerticalScrollbarSpacing(f32 value)
    {
        verticalScrollbarSpacing_ = value;
        SetDirty();
    }

    void ScrollRect::Rebuild(CanvasUpdate executing)
    {
        if (executing == CanvasUpdate::Prelayout)
            UpdateCachedData();
        if (executing == CanvasUpdate::PostLayout)
        {
            UpdateBounds();
            UpdateScrollbars(Vector2(0, 0));
            UpdatePrevData();
            hasRebuiltLayout_ = true;
        }
    }

    void ScrollRect::UpdateCachedData()
    {
        engine::Transform* own = engine::Component::transform();
        Scrollbar* h = horizontalScrollbar();
        Scrollbar* v = verticalScrollbar();
        horizontalScrollbarRect_ = engine::PPtr<engine::RectTransform>(h ? RectOf(h->engine::Component::transform()) : nullptr);
        verticalScrollbarRect_ = engine::PPtr<engine::RectTransform>(v ? RectOf(v->engine::Component::transform()) : nullptr);
        // True when they are children, or are not there at all.
        bool viewIsChild = viewRect()->parent() == own;
        bool hIsChild = !horizontalScrollbarRect_.Get() || horizontalScrollbarRect_->parent() == own;
        bool vIsChild = !verticalScrollbarRect_.Get() || verticalScrollbarRect_->parent() == own;
        bool allAreChildren = viewIsChild && hIsChild && vIsChild;
        hSliderExpand_ = allAreChildren && horizontalScrollbarRect_.Get() && horizontalScrollbarVisibility_ == ScrollbarVisibility::AutoHideAndExpandViewport;
        vSliderExpand_ = allAreChildren && verticalScrollbarRect_.Get() && verticalScrollbarVisibility_ == ScrollbarVisibility::AutoHideAndExpandViewport;
        hSliderHeight_ = horizontalScrollbarRect_.Get() ? horizontalScrollbarRect_->rect().height : 0;
        vSliderWidth_ = verticalScrollbarRect_.Get() ? verticalScrollbarRect_->rect().width : 0;
    }

    void ScrollRect::OnEnable()
    {
        UIBehaviour::OnEnable();
        if (Scrollbar* h = horizontalScrollbar())
            Listen(h, 0, true);
        if (Scrollbar* v = verticalScrollbar())
            Listen(v, 1, true);
        CanvasUpdateRegistry::RegisterCanvasElementForLayoutRebuildNative(this);
        SetDirty();
    }

    void ScrollRect::OnDisable()
    {
        CanvasUpdateRegistry::UnRegisterCanvasElementForRebuildNative(this);
        Listen(nullptr, 0, false);
        Listen(nullptr, 1, false);
        dragging_ = false;
        scrolling_ = false;
        hasRebuiltLayout_ = false;
        tracker_.Clear();
        velocity_ = Vector2(0, 0);
        LayoutRebuilder::MarkLayoutForRebuild(rectTransform());
        UIBehaviour::OnDisable();
    }

    bool ScrollRect::IsActive() const { return UIBehaviour::IsActive() && content_.Get(); }

    void ScrollRect::EnsureLayoutHasRebuilt()
    {
        if (!hasRebuiltLayout_ && !CanvasUpdateRegistry::IsRebuildingLayout())
            engine::Canvas::ForceUpdateCanvases();
    }

    void ScrollRect::OnScroll(PointerEventData* data)
    {
        if (!IsActive())
            return;
        EnsureLayoutHasRebuilt();
        UpdateBounds();
        Vector2 delta = data->scrollDelta();
        // The wheel's down is positive; the UI's up is.
        delta.y *= -1;
        if (vertical_ && !horizontal_)
        {
            if (std::fabs(delta.x) > std::fabs(delta.y))
                delta.y = delta.x;
            delta.x = 0;
        }
        if (horizontal_ && !vertical_)
        {
            if (std::fabs(delta.y) > std::fabs(delta.x))
                delta.x = delta.y;
            delta.y = 0;
        }
        if (data->IsScrolling())
            scrolling_ = true;
        engine::RectTransform* c = content_.Get();
        Vector2 position = c->anchoredPosition();
        position = Vector2(position.x + delta.x * scrollSensitivity_, position.y + delta.y * scrollSensitivity_);
        if (movementType_ == MovementType::Clamped)
        {
            Vector2 o = CalculateOffset(Vector2(position.x - c->anchoredPosition().x, position.y - c->anchoredPosition().y));
            position = Vector2(position.x + o.x, position.y + o.y);
        }
        SetContentAnchoredPosition(position);
        UpdateBounds();
    }

    void ScrollRect::OnInitializePotentialDrag(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left)
            return;
        velocity_ = Vector2(0, 0);
    }

    void ScrollRect::OnBeginDrag(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left || !IsActive())
            return;
        UpdateBounds();
        pointerStartLocalCursor_ = Vector2(0, 0);
        engine::RectTransformUtility::ScreenPointToLocalPointInRectangle(viewRect(), eventData->position(), eventData->pressEventCamera(), pointerStartLocalCursor_);
        contentStartPosition_ = content_->anchoredPosition();
        dragging_ = true;
    }

    void ScrollRect::OnEndDrag(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left)
            return;
        dragging_ = false;
    }

    void ScrollRect::OnDrag(PointerEventData* eventData)
    {
        if (!dragging_ || eventData->button() != PointerEventData::InputButton::Left || !IsActive())
            return;
        Vector2 localCursor;
        if (!engine::RectTransformUtility::ScreenPointToLocalPointInRectangle(viewRect(), eventData->position(), eventData->pressEventCamera(), localCursor))
            return;
        UpdateBounds();
        Vector2 pointerDelta(localCursor.x - pointerStartLocalCursor_.x, localCursor.y - pointerStartLocalCursor_.y);
        Vector2 position(contentStartPosition_.x + pointerDelta.x, contentStartPosition_.y + pointerDelta.y);
        // The offset that brings the content into the view.
        Vector2 now = content_->anchoredPosition();
        Vector2 offset = CalculateOffset(Vector2(position.x - now.x, position.y - now.y));
        position = Vector2(position.x + offset.x, position.y + offset.y);
        if (movementType_ == MovementType::Elastic)
        {
            if (offset.x != 0)
                position.x = position.x - RubberDelta(offset.x, viewBounds_.size().x);
            if (offset.y != 0)
                position.y = position.y - RubberDelta(offset.y, viewBounds_.size().y);
        }
        SetContentAnchoredPosition(position);
    }

    void ScrollRect::SetContentAnchoredPosition(Vector2 position)
    {
        engine::RectTransform* c = content_.Get();
        Vector2 now = c->anchoredPosition();
        if (!horizontal_)
            position.x = now.x;
        if (!vertical_)
            position.y = now.y;
        if (position != now)
        {
            c->SetAnchoredPosition(position);
            UpdateBounds();
        }
    }

    void ScrollRect::LateUpdate()
    {
        engine::RectTransform* c = content_.Get();
        if (!c)
            return;
        EnsureLayoutHasRebuilt();
        UpdateBounds();
        f32 deltaTime = engine::Time::unscaledDeltaTime();
        Vector2 offset = CalculateOffset(Vector2(0, 0));
        // A frame without time has no velocity to measure.
        if (deltaTime > 0)
        {
            if (!dragging_ && (offset != Vector2(0, 0) || velocity_ != Vector2(0, 0)))
            {
                Vector2 position = c->anchoredPosition();
                for (i32 axis = 0; axis < 2; axis++)
                {
                    if (movementType_ == MovementType::Elastic && offset[axis] != 0)
                    {
                        // A spring back into the view.
                        f32 speed = velocity_[axis];
                        f32 smoothTime = elasticity_;
                        if (scrolling_)
                            smoothTime *= 3;
                        f32 from = c->anchoredPosition()[axis];
                        position[axis] = mathf::SmoothDamp(from, from + offset[axis], speed, smoothTime, std::numeric_limits<f32>::infinity(), deltaTime);
                        if (std::fabs(speed) < 1)
                            speed = 0;
                        velocity_[axis] = speed;
                    }
                    else if (inertia_)
                    {
                        // The velocity decelerated.
                        velocity_[axis] *= std::pow(decelerationRate_, deltaTime);
                        if (std::fabs(velocity_[axis]) < 1)
                            velocity_[axis] = 0;
                        position[axis] += velocity_[axis] * deltaTime;
                    }
                    else
                        velocity_[axis] = 0;
                }
                if (movementType_ == MovementType::Clamped)
                {
                    Vector2 now = c->anchoredPosition();
                    Vector2 o = CalculateOffset(Vector2(position.x - now.x, position.y - now.y));
                    position = Vector2(position.x + o.x, position.y + o.y);
                }
                SetContentAnchoredPosition(position);
            }
            if (dragging_ && inertia_)
            {
                Vector2 now = c->anchoredPosition();
                Vector2 newVelocity((now.x - prevPosition_.x) / deltaTime, (now.y - prevPosition_.y) / deltaTime);
                f32 t = mathf::Clamp01(deltaTime * 10);
                velocity_ = Vector2(velocity_.x + (newVelocity.x - velocity_.x) * t, velocity_.y + (newVelocity.y - velocity_.y) * t);
            }
        }
        if (!SameBounds(viewBounds_, prevViewBounds_) || !SameBounds(contentBounds_, prevContentBounds_) || c->anchoredPosition() != prevPosition_)
        {
            UpdateScrollbars(offset);
            engine::UISystemProfilerApi::AddMarker("ScrollRect.value", this);
            onValueChanged_.Invoke(normalizedPosition());
            UpdatePrevData();
        }
        UpdateScrollbarVisibility();
        scrolling_ = false;
    }

    void ScrollRect::UpdatePrevData()
    {
        prevPosition_ = content_.Get() ? content_->anchoredPosition() : Vector2(0, 0);
        prevViewBounds_ = viewBounds_;
        prevContentBounds_ = contentBounds_;
    }

    void ScrollRect::UpdateScrollbars(Vector2 offset)
    {
        if (Scrollbar* h = horizontalScrollbar())
        {
            f32 contentWidth = contentBounds_.size().x;
            h->SetSize(contentWidth > 0 ? mathf::Clamp01((viewBounds_.size().x - std::fabs(offset.x)) / contentWidth) : 1);
            h->SetValue(horizontalNormalizedPosition());
        }
        if (Scrollbar* v = verticalScrollbar())
        {
            f32 contentHeight = contentBounds_.size().y;
            v->SetSize(contentHeight > 0 ? mathf::Clamp01((viewBounds_.size().y - std::fabs(offset.y)) / contentHeight) : 1);
            v->SetValue(verticalNormalizedPosition());
        }
    }

    void ScrollRect::SetNormalizedPosition(Vector2 value)
    {
        SetNormalizedPosition(value.x, 0);
        SetNormalizedPosition(value.y, 1);
    }

    f32 ScrollRect::horizontalNormalizedPosition()
    {
        UpdateBounds();
        f32 contentSize = contentBounds_.size().x, viewSize = viewBounds_.size().x;
        if (contentSize <= viewSize || mathf::Approximately(contentSize, viewSize))
            return viewBounds_.min().x > contentBounds_.min().x ? 1.0f : 0.0f;
        return (viewBounds_.min().x - contentBounds_.min().x) / (contentSize - viewSize);
    }

    f32 ScrollRect::verticalNormalizedPosition()
    {
        UpdateBounds();
        f32 contentSize = contentBounds_.size().y, viewSize = viewBounds_.size().y;
        if (contentSize <= viewSize || mathf::Approximately(contentSize, viewSize))
            return viewBounds_.min().y > contentBounds_.min().y ? 1.0f : 0.0f;
        return (viewBounds_.min().y - contentBounds_.min().y) / (contentSize - viewSize);
    }

    void ScrollRect::SetNormalizedPosition(f32 value, i32 axis)
    {
        EnsureLayoutHasRebuilt();
        UpdateBounds();
        engine::RectTransform* c = content_.Get();
        if (!c)
            return;
        // How much larger the content is than the view; where its lower corner goes in the view's space.
        f32 hiddenLength = contentBounds_.size()[axis] - viewBounds_.size()[axis];
        f32 contentBoundsMinPosition = viewBounds_.min()[axis] - value * hiddenLength;
        Vector2 anchored = c->anchoredPosition();
        f32 newAnchoredPosition = anchored[axis] + contentBoundsMinPosition - contentBounds_.min()[axis];
        if (std::fabs(anchored[axis] - newAnchoredPosition) > 0.01f)
        {
            anchored[axis] = newAnchoredPosition;
            c->SetAnchoredPosition(anchored);
            velocity_[axis] = 0;
            UpdateBounds();
        }
    }

    f32 ScrollRect::RubberDelta(f32 overStretching, f32 viewSize)
    {
        return (1 - (1 / ((std::fabs(overStretching) * 0.55f / viewSize) + 1))) * viewSize * (overStretching < 0 ? -1.0f : 1.0f);
    }

    bool ScrollRect::hScrollingNeeded() const { return contentBounds_.size().x > viewBounds_.size().x + 0.01f; }
    bool ScrollRect::vScrollingNeeded() const { return contentBounds_.size().y > viewBounds_.size().y + 0.01f; }

    void ScrollRect::SetLayoutHorizontal()
    {
        tracker_.Clear();
        UpdateCachedData();
        engine::RectTransform* view = viewRect();
        if (hSliderExpand_ || vSliderExpand_)
        {
            tracker_.Add(this, view,
                         Props({P::AnchorMinX, P::AnchorMinY, P::AnchorMaxX, P::AnchorMaxY, P::SizeDeltaX, P::SizeDeltaY, P::AnchoredPositionX, P::AnchoredPositionY}));
            // The view at full size: does the content fit without scrollbars?
            view->SetAnchorMin(Vector2(0, 0));
            view->SetAnchorMax(Vector2(1, 1));
            view->SetSizeDelta(Vector2(0, 0));
            view->SetAnchoredPosition(Vector2(0, 0));
            LayoutRebuilder::ForceRebuildLayoutImmediate(content_.Get());
            viewBounds_ = BoundsOfRect(view);
            contentBounds_ = GetBounds();
        }
        // Too tall: the vertical scrollbar, the view narrower by its room (the content may reflow taller).
        if (vSliderExpand_ && vScrollingNeeded())
        {
            view->SetSizeDelta(Vector2(-(vSliderWidth_ + verticalScrollbarSpacing_), view->sizeDelta().y));
            LayoutRebuilder::ForceRebuildLayoutImmediate(content_.Get());
            viewBounds_ = BoundsOfRect(view);
            contentBounds_ = GetBounds();
        }
        // Too wide: the horizontal scrollbar, the view lower by its room.
        if (hSliderExpand_ && hScrollingNeeded())
        {
            view->SetSizeDelta(Vector2(view->sizeDelta().x, -(hSliderHeight_ + horizontalScrollbarSpacing_)));
            viewBounds_ = BoundsOfRect(view);
            contentBounds_ = GetBounds();
        }
        // The horizontal bar may have made the vertical one needed after all.
        if (vSliderExpand_ && vScrollingNeeded() && view->sizeDelta().x == 0 && view->sizeDelta().y < 0)
            view->SetSizeDelta(Vector2(-(vSliderWidth_ + verticalScrollbarSpacing_), view->sizeDelta().y));
    }

    void ScrollRect::SetLayoutVertical()
    {
        UpdateScrollbarLayout();
        viewBounds_ = BoundsOfRect(viewRect());
        contentBounds_ = GetBounds();
    }

    void ScrollRect::UpdateScrollbarVisibility()
    {
        auto one = [](bool needed, bool axisEnabled, ScrollbarVisibility visibility, Scrollbar* bar) {
            if (!bar)
                return;
            bool show = visibility == ScrollbarVisibility::Permanent ? axisEnabled : needed;
            if (bar->gameObject()->activeSelf() != show)
                bar->gameObject()->SetActive(show);
        };
        one(vScrollingNeeded(), vertical_, verticalScrollbarVisibility_, verticalScrollbar());
        one(hScrollingNeeded(), horizontal_, horizontalScrollbarVisibility_, horizontalScrollbar());
    }

    void ScrollRect::UpdateScrollbarLayout()
    {
        if (vSliderExpand_ && horizontalScrollbar())
        {
            engine::RectTransform* r = horizontalScrollbarRect_.Get();
            tracker_.Add(this, r, Props({P::AnchorMinX, P::AnchorMaxX, P::SizeDeltaX, P::AnchoredPositionX}));
            r->SetAnchorMin(Vector2(0, r->anchorMin().y));
            r->SetAnchorMax(Vector2(1, r->anchorMax().y));
            r->SetAnchoredPosition(Vector2(0, r->anchoredPosition().y));
            r->SetSizeDelta(Vector2(vScrollingNeeded() ? -(vSliderWidth_ + verticalScrollbarSpacing_) : 0, r->sizeDelta().y));
        }
        if (hSliderExpand_ && verticalScrollbar())
        {
            engine::RectTransform* r = verticalScrollbarRect_.Get();
            tracker_.Add(this, r, Props({P::AnchorMinY, P::AnchorMaxY, P::SizeDeltaY, P::AnchoredPositionY}));
            r->SetAnchorMin(Vector2(r->anchorMin().x, 0));
            r->SetAnchorMax(Vector2(r->anchorMax().x, 1));
            r->SetAnchoredPosition(Vector2(r->anchoredPosition().x, 0));
            r->SetSizeDelta(Vector2(r->sizeDelta().x, hScrollingNeeded() ? -(hSliderHeight_ + horizontalScrollbarSpacing_) : 0));
        }
    }

    void ScrollRect::UpdateBounds()
    {
        viewBounds_ = BoundsOfRect(viewRect());
        contentBounds_ = GetBounds();
        engine::RectTransform* c = content_.Get();
        if (!c)
            return;
        Vector3 contentSize = contentBounds_.size();
        Vector3 contentPos = contentBounds_.center;
        Vector2 contentPivot = c->pivot();
        AdjustBounds(viewBounds_, contentPivot, contentSize, contentPos);
        contentBounds_.SetSize(contentSize);
        contentBounds_.center = contentPos;
        if (movementType_ == MovementType::Clamped)
        {
            // The content's ends never inside the view's (content that shrank): it moves back.
            Vector2 delta(0, 0);
            Vector3 vmin = viewBounds_.min(), vmax = viewBounds_.max(), cmin = contentBounds_.min(), cmax = contentBounds_.max();
            if (vmax.x > cmax.x)
                delta.x = std::fmin(vmin.x - cmin.x, vmax.x - cmax.x);
            else if (vmin.x < cmin.x)
                delta.x = std::fmax(vmin.x - cmin.x, vmax.x - cmax.x);
            if (vmin.y < cmin.y)
                delta.y = std::fmax(vmin.y - cmin.y, vmax.y - cmax.y);
            else if (vmax.y > cmax.y)
                delta.y = std::fmin(vmin.y - cmin.y, vmax.y - cmax.y);
            if (delta.x * delta.x + delta.y * delta.y > std::numeric_limits<f32>::denorm_min())
            {
                Vector2 a = c->anchoredPosition();
                contentPos = Vector3(a.x + delta.x, a.y + delta.y, 0);
                if (!horizontal_)
                    contentPos.x = a.x;
                if (!vertical_)
                    contentPos.y = a.y;
                AdjustBounds(viewBounds_, contentPivot, contentSize, contentPos);
            }
        }
    }

    void ScrollRect::AdjustBounds(Bounds& viewBounds, Vector2& contentPivot, Vector3& contentSize, Vector3& contentPos)
    {
        // The content at least as large as the view, grown from its pivot (top pivot: downwards).
        Vector3 excess = viewBounds.size() - contentSize;
        if (excess.x > 0)
        {
            contentPos.x -= excess.x * (contentPivot.x - 0.5f);
            contentSize.x = viewBounds.size().x;
        }
        if (excess.y > 0)
        {
            contentPos.y -= excess.y * (contentPivot.y - 0.5f);
            contentSize.y = viewBounds.size().y;
        }
    }

    Bounds ScrollRect::GetBounds() const
    {
        engine::RectTransform* c = content_.Get();
        if (!c)
            return Bounds();
        Vector3 corners[4];
        c->GetWorldCorners(corners);
        return InternalGetBounds(corners, viewRect()->worldToLocalMatrix());
    }

    Bounds ScrollRect::InternalGetBounds(const Vector3 (&corners)[4], const Matrix4x4& viewWorldToLocalMatrix)
    {
        f32 big = std::numeric_limits<f32>::max();
        Vector3 vmin(big, big, big), vmax(-big, -big, -big);
        for (const Vector3& corner : corners)
        {
            Vector3 v = viewWorldToLocalMatrix.MultiplyPoint3x4(corner);
            vmin = Vector3(std::fmin(v.x, vmin.x), std::fmin(v.y, vmin.y), std::fmin(v.z, vmin.z));
            vmax = Vector3(std::fmax(v.x, vmax.x), std::fmax(v.y, vmax.y), std::fmax(v.z, vmax.z));
        }
        Bounds b(vmin, Vector3(0, 0, 0));
        b.Encapsulate(vmax);
        return b;
    }

    Vector2 ScrollRect::CalculateOffset(Vector2 delta) const
    {
        return InternalCalculateOffset(viewBounds_, contentBounds_, horizontal_, vertical_, movementType_, delta);
    }

    Vector2 ScrollRect::InternalCalculateOffset(const Bounds& viewBounds, const Bounds& contentBounds, bool horizontal, bool vertical, MovementType movementType,
                                                Vector2 delta)
    {
        Vector2 offset(0, 0);
        if (movementType == MovementType::Unrestricted)
            return offset;
        Vector3 cmin = contentBounds.min(), cmax = contentBounds.max(), vmin = viewBounds.min(), vmax = viewBounds.max();
        // Small offsets are none (no layout every frame for rounding).
        if (horizontal)
        {
            f32 maxOffset = vmax.x - (cmax.x + delta.x);
            f32 minOffset = vmin.x - (cmin.x + delta.x);
            if (minOffset < -0.001f)
                offset.x = minOffset;
            else if (maxOffset > 0.001f)
                offset.x = maxOffset;
        }
        if (vertical)
        {
            f32 maxOffset = vmax.y - (cmax.y + delta.y);
            f32 minOffset = vmin.y - (cmin.y + delta.y);
            if (maxOffset > 0.001f)
                offset.y = maxOffset;
            else if (minOffset < -0.001f)
                offset.y = minOffset;
        }
        return offset;
    }

    void ScrollRect::SetDirty()
    {
        if (!IsActive())
            return;
        LayoutRebuilder::MarkLayoutForRebuild(rectTransform());
    }

    void ScrollRect::SetDirtyCaching()
    {
        if (!IsActive())
            return;
        CanvasUpdateRegistry::RegisterCanvasElementForLayoutRebuildNative(this);
        LayoutRebuilder::MarkLayoutForRebuild(rectTransform());
        viewRect_ = engine::PPtr<engine::RectTransform>();
    }

    bool ScrollRect::Handles(EventKind kind) const
    {
        return kind == EventKind::InitializePotentialDrag || kind == EventKind::BeginDrag || kind == EventKind::EndDrag || kind == EventKind::Drag ||
               kind == EventKind::Scroll;
    }

    void ScrollRect::HandleEvent(EventKind kind, BaseEventData* data)
    {
        auto* p = static_cast<PointerEventData*>(data);
        switch (kind)
        {
        case EventKind::InitializePotentialDrag: OnInitializePotentialDrag(p); break;
        case EventKind::BeginDrag: OnBeginDrag(p); break;
        case EventKind::EndDrag: OnEndDrag(p); break;
        case EventKind::Drag: OnDrag(p); break;
        case EventKind::Scroll: OnScroll(p); break;
        default: break;
        }
    }

    void ScrollRect::OnAfterCloneNative()
    {
        UIBehaviour::OnAfterCloneNative();
        tracker_ = engine::DrivenRectTransformTracker();
        listenedH_ = engine::PPtr<engine::Object>();
        listenedV_ = engine::PPtr<engine::Object>();
    }
}
