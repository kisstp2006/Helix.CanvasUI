#include <Helix/CanvasUI/Controls/Sliders.h>

#include <Helix/CanvasUI/Rendering/Image.h>
#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Transform/RectTransformUtility.h>
#include <Helix/Math/Mathf.h>

#include <cmath>

namespace helix::canvasui
{
    namespace
    {
        using P = engine::DrivenTransformProperties;

        engine::RectTransform* RectOf(engine::Transform* t) { return t && t->Is<engine::RectTransform>() ? static_cast<engine::RectTransform*>(t) : nullptr; }

        // The pointer's position for a drag (one display: the event's).
        bool DragPosition(PointerEventData* eventData, Vector2& position)
        {
            position = eventData->position();
            return true;
        }
    }

    // ----------------------------------------------------------------------------------------------------- Slider

    Slider::Slider()
    {
        static bool registered = false;
        if (!registered)
        {
            registered = true;
            engine::NativeEventEntry e;
            e.storage = [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<Slider*>(o)->onValueChanged_; };
            e.raise = [](engine::Object* o, Span<const scripting::Variant> a) {
                f32 v = 0;
                if (!a.Empty())
                    a[0].Get(v);
                static_cast<Slider*>(o)->onValueChanged_.Invoke(v);
            };
            engine::RegisterNativeEvent(scripting::TypeOf<Slider>(), 0, e);
        }
    }

    void Slider::SetFillRect(engine::RectTransform* value)
    {
        if (fillRect_.Get() == value)
            return;
        fillRect_ = engine::PPtr<engine::RectTransform>(value);
        UpdateCachedReferences();
        UpdateVisuals();
    }

    void Slider::SetHandleRect(engine::RectTransform* value)
    {
        if (handleRect_.Get() == value)
            return;
        handleRect_ = engine::PPtr<engine::RectTransform>(value);
        UpdateCachedReferences();
        UpdateVisuals();
    }

    void Slider::SetDirection(Direction value)
    {
        if (direction_ == value)
            return;
        direction_ = value;
        UpdateVisuals();
    }

    void Slider::SetMinValue(f32 value)
    {
        if (minValue_ == value)
            return;
        minValue_ = value;
        Set(value_, true);
        UpdateVisuals();
    }

    void Slider::SetMaxValue(f32 value)
    {
        if (maxValue_ == value)
            return;
        maxValue_ = value;
        Set(value_, true);
        UpdateVisuals();
    }

    void Slider::SetWholeNumbers(bool value)
    {
        if (wholeNumbers_ == value)
            return;
        wholeNumbers_ = value;
        Set(value_, true);
        UpdateVisuals();
    }

    f32 Slider::value() const { return wholeNumbers_ ? mathf::Round(value_) : value_; }

    f32 Slider::normalizedValue() const
    {
        if (mathf::Approximately(minValue_, maxValue_))
            return 0;
        return mathf::InverseLerp(minValue_, maxValue_, value());
    }

    void Slider::SetNormalizedValue(f32 value) { SetValue(mathf::Lerp(minValue_, maxValue_, value)); }

    void Slider::OnEnable()
    {
        Selectable::OnEnable();
        UpdateCachedReferences();
        Set(value_, false);
        // The visuals now (a drivers' change from the reload would have them stale).
        UpdateVisuals();
    }

    void Slider::OnDisable()
    {
        tracker_.Clear();
        Selectable::OnDisable();
    }

    void Slider::Update()
    {
        if (delayedUpdateVisuals_)
        {
            delayedUpdateVisuals_ = false;
            Set(value_, false);
            UpdateVisuals();
        }
    }

    void Slider::OnDidApplyAnimationProperties()
    {
        // An animation may have moved the fill or the handle: the value they show, its event when it changed.
        value_ = ClampValue(value_);
        f32 oldNormalizedValue = normalizedValue();
        i32 a = axis();
        auto* fillImage = static_cast<Image*>(fillImage_.Get());
        if (fillContainerRect_.Get())
        {
            if (fillImage && fillImage->type() == Image::Type::Filled)
                oldNormalizedValue = fillImage->fillAmount();
            else
                oldNormalizedValue = reverseValue() ? 1 - fillRect_->anchorMin()[a] : fillRect_->anchorMax()[a];
        }
        else if (handleContainerRect_.Get())
            oldNormalizedValue = reverseValue() ? 1 - handleRect_->anchorMin()[a] : handleRect_->anchorMin()[a];
        UpdateVisuals();
        if (oldNormalizedValue != normalizedValue())
        {
            engine::UISystemProfilerApi::AddMarker("Slider.value", this);
            onValueChanged_.Invoke(value_);
        }
        Selectable::OnDidApplyAnimationProperties();
    }

    void Slider::UpdateCachedReferences()
    {
        engine::RectTransform* own = RectOf(engine::Component::transform());
        engine::RectTransform* fill = fillRect_.Get();
        if (fill && fill != own)
        {
            fillImage_ = engine::PPtr<engine::Object>(ComponentOf<Image>(fill->gameObject()));
            fillContainerRect_ = engine::PPtr<engine::RectTransform>(RectOf(fill->parent()));
        }
        else
        {
            fillRect_ = engine::PPtr<engine::RectTransform>();
            fillContainerRect_ = engine::PPtr<engine::RectTransform>();
            fillImage_ = engine::PPtr<engine::Object>();
        }
        engine::RectTransform* handle = handleRect_.Get();
        if (handle && handle != own)
            handleContainerRect_ = engine::PPtr<engine::RectTransform>(RectOf(handle->parent()));
        else
        {
            handleRect_ = engine::PPtr<engine::RectTransform>();
            handleContainerRect_ = engine::PPtr<engine::RectTransform>();
        }
    }

    f32 Slider::ClampValue(f32 input) const
    {
        f32 v = mathf::Clamp(input, minValue_, maxValue_);
        return wholeNumbers_ ? mathf::Round(v) : v;
    }

    void Slider::Set(f32 input, bool sendCallback)
    {
        f32 newValue = ClampValue(input);
        if (value_ == newValue)
            return;
        value_ = newValue;
        UpdateVisuals();
        if (sendCallback)
        {
            engine::UISystemProfilerApi::AddMarker("Slider.value", this);
            onValueChanged_.Invoke(newValue);
        }
    }

    void Slider::OnRectTransformDimensionsChange()
    {
        Selectable::OnRectTransformDimensionsChange();
        // The layout of the hierarchy may not be done yet: a disabled slider updates when enabled.
        if (!IsActive())
            return;
        UpdateVisuals();
    }

    void Slider::UpdateVisuals()
    {
        tracker_.Clear();
        i32 a = axis();
        f32 n = normalizedValue();
        if (fillContainerRect_.Get() && fillRect_.Get())
        {
            tracker_.Add(this, fillRect_.Get(), P::Anchors);
            Vector2 anchorMin(0, 0), anchorMax(1, 1);
            auto* fillImage = static_cast<Image*>(fillImage_.Get());
            if (fillImage && fillImage->type() == Image::Type::Filled)
                fillImage->SetFillAmount(n);
            else if (reverseValue())
                anchorMin[a] = 1 - n;
            else
                anchorMax[a] = n;
            fillRect_->SetAnchorMin(anchorMin);
            fillRect_->SetAnchorMax(anchorMax);
        }
        if (handleContainerRect_.Get() && handleRect_.Get())
        {
            tracker_.Add(this, handleRect_.Get(), P::Anchors);
            Vector2 anchorMin(0, 0), anchorMax(1, 1);
            anchorMin[a] = anchorMax[a] = reverseValue() ? 1 - n : n;
            handleRect_->SetAnchorMin(anchorMin);
            handleRect_->SetAnchorMax(anchorMax);
        }
    }

    void Slider::UpdateDrag(PointerEventData* eventData, engine::Camera* camera)
    {
        engine::RectTransform* clickRect = handleContainerRect_.Get() ? handleContainerRect_.Get() : fillContainerRect_.Get();
        i32 a = axis();
        if (!clickRect || clickRect->rect().size()[a] <= 0)
            return;
        Vector2 position;
        if (!DragPosition(eventData, position))
            return;
        Vector2 localCursor;
        if (!engine::RectTransformUtility::ScreenPointToLocalPointInRectangle(clickRect, position, camera, localCursor))
            return;
        Rect r = clickRect->rect();
        localCursor = Vector2(localCursor.x - r.x, localCursor.y - r.y);
        f32 v = mathf::Clamp01((localCursor[a] - offset_[a]) / r.size()[a]);
        SetNormalizedValue(reverseValue() ? 1 - v : v);
    }

    bool Slider::MayDrag(PointerEventData* eventData) const
    {
        return IsActive() && IsInteractable() && eventData->button() == PointerEventData::InputButton::Left;
    }

    void Slider::OnPointerDown(PointerEventData* eventData)
    {
        if (!MayDrag(eventData))
            return;
        Selectable::OnPointerDown(eventData);
        offset_ = Vector2(0, 0);
        Vector2 at = eventData->pointerPressRaycast.screenPosition;
        if (handleContainerRect_.Get() && engine::RectTransformUtility::RectangleContainsScreenPoint(handleRect_.Get(), at, eventData->enterEventCamera()))
        {
            // On the handle: the drag keeps the point of the handle it took.
            Vector2 local;
            if (engine::RectTransformUtility::ScreenPointToLocalPointInRectangle(handleRect_.Get(), at, eventData->pressEventCamera(), local))
                offset_ = local;
        }
        else
            // Beside it: the value jumps to the point.
            UpdateDrag(eventData, eventData->pressEventCamera());
    }

    void Slider::OnDrag(PointerEventData* eventData)
    {
        if (!MayDrag(eventData))
            return;
        UpdateDrag(eventData, eventData->pressEventCamera());
    }

    void Slider::OnMove(AxisEventData* eventData)
    {
        if (!IsActive() || !IsInteractable())
        {
            Selectable::OnMove(eventData);
            return;
        }
        // Along its axis (with nothing to select that way) navigation moves the value a step.
        f32 v = value(), step = stepSize();
        switch (eventData->moveDir())
        {
        case MoveDirection::Left:
            if (axis() == 0 && !FindSelectableOnLeft())
                Set(reverseValue() ? v + step : v - step, true);
            else
                Selectable::OnMove(eventData);
            break;
        case MoveDirection::Right:
            if (axis() == 0 && !FindSelectableOnRight())
                Set(reverseValue() ? v - step : v + step, true);
            else
                Selectable::OnMove(eventData);
            break;
        case MoveDirection::Up:
            if (axis() == 1 && !FindSelectableOnUp())
                Set(reverseValue() ? v - step : v + step, true);
            else
                Selectable::OnMove(eventData);
            break;
        case MoveDirection::Down:
            if (axis() == 1 && !FindSelectableOnDown())
                Set(reverseValue() ? v + step : v - step, true);
            else
                Selectable::OnMove(eventData);
            break;
        default: break;
        }
    }

    Selectable* Slider::FindSelectableOnLeft()
    {
        if (NavigationOf().mode == NavigationMode::Automatic && axis() == 0)
            return nullptr;
        return Selectable::FindSelectableOnLeft();
    }

    Selectable* Slider::FindSelectableOnRight()
    {
        if (NavigationOf().mode == NavigationMode::Automatic && axis() == 0)
            return nullptr;
        return Selectable::FindSelectableOnRight();
    }

    Selectable* Slider::FindSelectableOnUp()
    {
        if (NavigationOf().mode == NavigationMode::Automatic && axis() == 1)
            return nullptr;
        return Selectable::FindSelectableOnUp();
    }

    Selectable* Slider::FindSelectableOnDown()
    {
        if (NavigationOf().mode == NavigationMode::Automatic && axis() == 1)
            return nullptr;
        return Selectable::FindSelectableOnDown();
    }

    void Slider::OnInitializePotentialDrag(PointerEventData* eventData) { eventData->SetUseDragThreshold(false); }

    void Slider::SetDirection(Direction direction, bool includeRectLayouts)
    {
        i32 oldAxis = axis();
        bool oldReverse = reverseValue();
        SetDirection(direction);
        if (!includeRectLayouts)
            return;
        engine::RectTransform* own = RectOf(engine::Component::transform());
        if (axis() != oldAxis)
            engine::RectTransformUtility::FlipLayoutAxes(own, true, true);
        if (reverseValue() != oldReverse)
            engine::RectTransformUtility::FlipLayoutOnAxis(own, axis(), true, true);
    }

    void Slider::HandleMore(EventKind kind, BaseEventData* data)
    {
        if (kind == EventKind::Drag)
            OnDrag(static_cast<PointerEventData*>(data));
        else if (kind == EventKind::InitializePotentialDrag)
            OnInitializePotentialDrag(static_cast<PointerEventData*>(data));
    }

    // -------------------------------------------------------------------------------------------------- Scrollbar

    Scrollbar::Scrollbar()
    {
        static bool registered = false;
        if (!registered)
        {
            registered = true;
            engine::NativeEventEntry e;
            e.storage = [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<Scrollbar*>(o)->onValueChanged_; };
            e.raise = [](engine::Object* o, Span<const scripting::Variant> a) {
                f32 v = 0;
                if (!a.Empty())
                    a[0].Get(v);
                static_cast<Scrollbar*>(o)->onValueChanged_.Invoke(v);
            };
            engine::RegisterNativeEvent(scripting::TypeOf<Scrollbar>(), 0, e);
        }
    }

    void Scrollbar::SetHandleRect(engine::RectTransform* value)
    {
        if (handleRect_.Get() == value)
            return;
        handleRect_ = engine::PPtr<engine::RectTransform>(value);
        UpdateCachedReferences();
        UpdateVisuals();
    }

    void Scrollbar::SetDirection(Direction value)
    {
        if (direction_ == value)
            return;
        direction_ = value;
        UpdateVisuals();
    }

    f32 Scrollbar::value() const
    {
        f32 v = value_;
        if (numberOfSteps_ > 1)
            v = mathf::Round(v * f32(numberOfSteps_ - 1)) / f32(numberOfSteps_ - 1);
        return v;
    }

    void Scrollbar::SetSize(f32 value)
    {
        f32 v = mathf::Clamp01(value);
        if (size_ == v)
            return;
        size_ = v;
        UpdateVisuals();
    }

    void Scrollbar::SetNumberOfSteps(i32 value)
    {
        if (numberOfSteps_ == value)
            return;
        numberOfSteps_ = value;
        Set(value_, true);
        UpdateVisuals();
    }

    void Scrollbar::OnEnable()
    {
        Selectable::OnEnable();
        UpdateCachedReferences();
        Set(value_, false);
        UpdateVisuals();
    }

    void Scrollbar::OnDisable()
    {
        tracker_.Clear();
        Selectable::OnDisable();
    }

    void Scrollbar::Update()
    {
        if (delayedUpdateVisuals_)
        {
            delayedUpdateVisuals_ = false;
            UpdateVisuals();
        }
    }

    void Scrollbar::UpdateCachedReferences()
    {
        engine::RectTransform* handle = handleRect_.Get();
        containerRect_ = engine::PPtr<engine::RectTransform>(handle ? RectOf(handle->parent()) : nullptr);
    }

    void Scrollbar::Set(f32 input, bool sendCallback)
    {
        // Unclamped: a scroll view's inertia takes the content past its ends (the handle shows the clamped value).
        f32 current = value();
        value_ = input;
        if (current == value())
            return;
        UpdateVisuals();
        if (sendCallback)
        {
            engine::UISystemProfilerApi::AddMarker("Scrollbar.value", this);
            onValueChanged_.Invoke(value());
        }
    }

    void Scrollbar::OnRectTransformDimensionsChange()
    {
        Selectable::OnRectTransformDimensionsChange();
        if (!IsActive())
            return;
        UpdateVisuals();
    }

    void Scrollbar::OnDidApplyAnimationProperties()
    {
        UpdateVisuals();
        Selectable::OnDidApplyAnimationProperties();
    }

    void Scrollbar::UpdateVisuals()
    {
        tracker_.Clear();
        engine::RectTransform* handle = handleRect_.Get();
        if (!containerRect_.Get() || !handle)
            return;
        tracker_.Add(this, handle, P::Anchors);
        Vector2 anchorMin(0, 0), anchorMax(1, 1);
        i32 a = axis();
        f32 movement = mathf::Clamp01(value()) * (1 - size_);
        if (reverseValue())
        {
            anchorMin[a] = 1 - movement - size_;
            anchorMax[a] = 1 - movement;
        }
        else
        {
            anchorMin[a] = movement;
            anchorMax[a] = movement + size_;
        }
        handle->SetAnchorMin(anchorMin);
        handle->SetAnchorMax(anchorMax);
    }

    void Scrollbar::UpdateDrag(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left || !containerRect_.Get())
            return;
        Vector2 position;
        if (!DragPosition(eventData, position))
            return;
        UpdateDrag(containerRect_.Get(), position, eventData->pressEventCamera());
    }

    void Scrollbar::UpdateDrag(engine::RectTransform* containerRect, Vector2 position, engine::Camera* camera)
    {
        Vector2 localCursor;
        if (!engine::RectTransformUtility::ScreenPointToLocalPointInRectangle(containerRect, position, camera, localCursor))
            return;
        Rect c = containerRect->rect();
        Vector2 handleCenter(localCursor.x - offset_.x - c.x, localCursor.y - offset_.y - c.y);
        engine::RectTransform* handle = handleRect_.Get();
        Vector2 hs = handle->rect().size(), sd = handle->sizeDelta();
        Vector2 handleCorner(handleCenter.x - (hs.x - sd.x) * 0.5f, handleCenter.y - (hs.y - sd.y) * 0.5f);
        f32 parentSize = axis() == 0 ? c.width : c.height;
        f32 remainingSize = parentSize * (1 - size_);
        if (remainingSize <= 0)
            return;
        DoUpdateDrag(handleCorner, remainingSize);
    }

    void Scrollbar::DoUpdateDrag(Vector2 handleCorner, f32 remainingSize)
    {
        switch (direction_)
        {
        case Direction::LeftToRight: Set(mathf::Clamp01(handleCorner.x / remainingSize), true); break;
        case Direction::RightToLeft: Set(mathf::Clamp01(1 - handleCorner.x / remainingSize), true); break;
        case Direction::BottomToTop: Set(mathf::Clamp01(handleCorner.y / remainingSize), true); break;
        case Direction::TopToBottom: Set(mathf::Clamp01(1 - handleCorner.y / remainingSize), true); break;
        }
    }

    bool Scrollbar::MayDrag(PointerEventData* eventData) const
    {
        return IsActive() && IsInteractable() && eventData->button() == PointerEventData::InputButton::Left;
    }

    void Scrollbar::OnBeginDrag(PointerEventData* eventData)
    {
        isPointerDownAndNotDragging_ = false;
        if (!MayDrag(eventData) || !containerRect_.Get())
            return;
        offset_ = Vector2(0, 0);
        engine::RectTransform* handle = handleRect_.Get();
        Vector2 at = eventData->pointerPressRaycast.screenPosition;
        if (engine::RectTransformUtility::RectangleContainsScreenPoint(handle, at, eventData->enterEventCamera()))
        {
            Vector2 local;
            if (engine::RectTransformUtility::ScreenPointToLocalPointInRectangle(handle, at, eventData->pressEventCamera(), local))
            {
                Vector2 center = handle->rect().center();
                offset_ = Vector2(local.x - center.x, local.y - center.y);
            }
        }
    }

    void Scrollbar::OnDrag(PointerEventData* eventData)
    {
        if (!MayDrag(eventData))
            return;
        if (containerRect_.Get())
            UpdateDrag(eventData);
    }

    void Scrollbar::OnPointerDown(PointerEventData* eventData)
    {
        if (!MayDrag(eventData))
            return;
        Selectable::OnPointerDown(eventData);
        isPointerDownAndNotDragging_ = true;
        pointerDownRepeat_ = StartCoroutine(ClickRepeat(eventData->pointerPressRaycast.screenPosition, eventData->enterEventCamera()));
    }

    engine::Coroutine Scrollbar::ClickRepeat(Vector2 screenPosition, engine::Camera* camera)
    {
        while (isPointerDownAndNotDragging_)
        {
            engine::RectTransform* handle = handleRect_.Get();
            if (handle && !engine::RectTransformUtility::RectangleContainsScreenPoint(handle, screenPosition, camera))
            {
                Vector2 local;
                if (engine::RectTransformUtility::ScreenPointToLocalPointInRectangle(handle, screenPosition, camera, local))
                {
                    // A page (the handle's size) toward the point.
                    f32 axisCoordinate = axis() == 0 ? local.x : local.y;
                    f32 change = axisCoordinate < 0 ? size_ : -size_;
                    f32 v = value() + (reverseValue() ? change : -change);
                    v = mathf::Clamp01(v);
                    // Four decimals.
                    SetValue(mathf::Round(v * 10000.0f) / 10000.0f);
                }
            }
            co_await engine::WaitForEndOfFrame();
        }
    }

    void Scrollbar::OnPointerUp(PointerEventData* eventData)
    {
        Selectable::OnPointerUp(eventData);
        isPointerDownAndNotDragging_ = false;
    }

    void Scrollbar::OnMove(AxisEventData* eventData)
    {
        if (!IsActive() || !IsInteractable())
        {
            Selectable::OnMove(eventData);
            return;
        }
        f32 v = value(), step = stepSize();
        switch (eventData->moveDir())
        {
        case MoveDirection::Left:
            if (axis() == 0 && !FindSelectableOnLeft())
                Set(mathf::Clamp01(reverseValue() ? v + step : v - step), true);
            else
                Selectable::OnMove(eventData);
            break;
        case MoveDirection::Right:
            if (axis() == 0 && !FindSelectableOnRight())
                Set(mathf::Clamp01(reverseValue() ? v - step : v + step), true);
            else
                Selectable::OnMove(eventData);
            break;
        case MoveDirection::Up:
            if (axis() == 1 && !FindSelectableOnUp())
                Set(mathf::Clamp01(reverseValue() ? v - step : v + step), true);
            else
                Selectable::OnMove(eventData);
            break;
        case MoveDirection::Down:
            if (axis() == 1 && !FindSelectableOnDown())
                Set(mathf::Clamp01(reverseValue() ? v + step : v - step), true);
            else
                Selectable::OnMove(eventData);
            break;
        default: break;
        }
    }

    Selectable* Scrollbar::FindSelectableOnLeft()
    {
        if (NavigationOf().mode == NavigationMode::Automatic && axis() == 0)
            return nullptr;
        return Selectable::FindSelectableOnLeft();
    }

    Selectable* Scrollbar::FindSelectableOnRight()
    {
        if (NavigationOf().mode == NavigationMode::Automatic && axis() == 0)
            return nullptr;
        return Selectable::FindSelectableOnRight();
    }

    Selectable* Scrollbar::FindSelectableOnUp()
    {
        if (NavigationOf().mode == NavigationMode::Automatic && axis() == 1)
            return nullptr;
        return Selectable::FindSelectableOnUp();
    }

    Selectable* Scrollbar::FindSelectableOnDown()
    {
        if (NavigationOf().mode == NavigationMode::Automatic && axis() == 1)
            return nullptr;
        return Selectable::FindSelectableOnDown();
    }

    void Scrollbar::OnInitializePotentialDrag(PointerEventData* eventData) { eventData->SetUseDragThreshold(false); }

    void Scrollbar::SetDirection(Direction direction, bool includeRectLayouts)
    {
        i32 oldAxis = axis();
        bool oldReverse = reverseValue();
        SetDirection(direction);
        if (!includeRectLayouts)
            return;
        engine::RectTransform* own = RectOf(engine::Component::transform());
        if (axis() != oldAxis)
            engine::RectTransformUtility::FlipLayoutAxes(own, true, true);
        if (reverseValue() != oldReverse)
            engine::RectTransformUtility::FlipLayoutOnAxis(own, axis(), true, true);
    }

    void Scrollbar::HandleMore(EventKind kind, BaseEventData* data)
    {
        if (kind == EventKind::BeginDrag)
            OnBeginDrag(static_cast<PointerEventData*>(data));
        else if (kind == EventKind::Drag)
            OnDrag(static_cast<PointerEventData*>(data));
        else if (kind == EventKind::InitializePotentialDrag)
            OnInitializePotentialDrag(static_cast<PointerEventData*>(data));
    }

    void Slider::OnAfterCloneNative()
    {
        Selectable::OnAfterCloneNative();
        tracker_ = engine::DrivenRectTransformTracker();
    }

    void Scrollbar::OnAfterCloneNative()
    {
        Selectable::OnAfterCloneNative();
        tracker_ = engine::DrivenRectTransformTracker();
        pointerDownRepeat_ = engine::CoroutineRef();
        isPointerDownAndNotDragging_ = false;
    }
}
