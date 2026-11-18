#pragma once

#include <Helix/CanvasUI/Core/OwnedRoot.h>
#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/UI/Engine/GuiTypes.h>
#include <Helix/Math/Geometry.h>

// The event system's data: what a pointer, a move or a selection event carries to the handlers, and what a raycast
// found (RaycastResult). The data objects belong to the input modules, which reuse them frame after frame.
namespace helix::canvasui
{
    class InputModule;
    class BaseRaycaster;
    class EventSystem;

    HX_ENUM(Namespace = "HelixEngine.UIEvents")
    enum class MoveDirection : i32
    {
        Left = 0,
        Up = 1,
        Right = 2,
        Down = 3,
        None = 4,
    };

    HX_ENUM(Namespace = "HelixEngine.UIEvents", Attributes = "[System.Flags]")
    enum class EventHandle : i32
    {
        Unused = 0,
        Used = 1,
    };

    // What a raycast hit: the object, the raycaster that found it, where, and how it sorts.
    struct RaycastResult
    {
        engine::PPtr<engine::GameObject> gameObject;
        engine::PPtr<engine::Object> module;
        f32 distance = 0;
        f32 index = 0;
        i32 depth = 0;
        i32 sortingGroupID = 0;
        i32 sortingGroupOrder = 0;
        i32 sortingLayer = 0;
        i32 sortingOrder = 0;
        Vector3 origin = Vector3(0, 0, 0);
        Vector3 worldPosition = Vector3(0, 0, 0);
        Vector3 worldNormal = Vector3(0, 0, 0);
        Vector2 screenPosition = Vector2(0, 0);
        i32 displayIndex = 0;

        bool isValid() const;
        void Clear() { *this = RaycastResult(); }
    };

    // RaycastResult across the bindings (C# has its own struct over it): the objects by their ids.
    HX_STRUCT(Namespace = "HelixEngine.UIEvents", Internal)
    struct RaycastResultData
    {
        HX_FIELD() engine::EntityId gameObject;
        HX_FIELD() engine::EntityId module;
        HX_FIELD() f32 distance = 0;
        HX_FIELD() f32 index = 0;
        HX_FIELD() i32 depth = 0;
        HX_FIELD() i32 sortingGroupID = 0;
        HX_FIELD() i32 sortingGroupOrder = 0;
        HX_FIELD() i32 sortingLayer = 0;
        HX_FIELD() i32 sortingOrder = 0;
        HX_FIELD() Vector3 origin = Vector3(0, 0, 0);
        HX_FIELD() Vector3 worldPosition = Vector3(0, 0, 0);
        HX_FIELD() Vector3 worldNormal = Vector3(0, 0, 0);
        HX_FIELD() Vector2 screenPosition = Vector2(0, 0);
        HX_FIELD() i32 displayIndex = 0;

        static RaycastResultData From(const RaycastResult& r);
        RaycastResult ToResult() const;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents", Root, Owned, Abstract)
    class AbstractEventData : public OwnedRoot<AbstractEventData>
    {
    public:
        AbstractEventData() = default;
        ~AbstractEventData() override = default;

        HX_PROPERTY(ReadOnly) virtual bool used() const { return used_; }
        HX_FUNCTION() virtual void Use() { used_ = true; }
        HX_FUNCTION() virtual void Reset() { used_ = false; }

        scripting::TypeId HelixType() const override { return scripting::TypeOf<AbstractEventData>(); }

    protected:
        bool used_ = false;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents")
    class BaseEventData : public AbstractEventData
    {
    public:
        HX_FUNCTION(Constructor) explicit BaseEventData(EventSystem* eventSystem);
        ~BaseEventData() override = default;

        HX_PROPERTY(ReadOnly) InputModule* currentInputModule() const;
        HX_PROPERTY() engine::GameObject* selectedObject() const;
        void SetSelectedObject(engine::GameObject* value);
        EventSystem* eventSystem() const;

        scripting::TypeId HelixType() const override { return scripting::TypeOf<BaseEventData>(); }

    private:
        engine::PPtr<engine::Object> eventSystem_;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents")
    class AxisEventData : public BaseEventData
    {
    public:
        HX_FUNCTION(Constructor) explicit AxisEventData(EventSystem* eventSystem) : BaseEventData(eventSystem) {}

        HX_PROPERTY() Vector2 moveVector() const { return moveVector_; }
        void SetMoveVector(Vector2 value) { moveVector_ = value; }
        HX_PROPERTY() MoveDirection moveDir() const { return moveDir_; }
        void SetMoveDir(MoveDirection value) { moveDir_ = value; }

        scripting::TypeId HelixType() const override { return scripting::TypeOf<AxisEventData>(); }

    private:
        Vector2 moveVector_ = Vector2(0, 0);
        MoveDirection moveDir_ = MoveDirection::None;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents")
    class PointerEventData : public BaseEventData
    {
    public:
        HX_ENUM()
        enum class InputButton : i32
        {
            Left = 0,
            Right = 1,
            Middle = 2,
        };
        HX_ENUM()
        enum class FramePressState : i32
        {
            Pressed = 0,
            Released = 1,
            PressedAndReleased = 2,
            NotChanged = 3,
        };

        HX_FUNCTION(Constructor) explicit PointerEventData(EventSystem* eventSystem);

        // The objects under the pointer it entered (the hovered chain).
        HX_FUNCTION(Internal) Array<engine::GameObject*> HoveredInternal() const;
        HX_FUNCTION(Internal) void SetHoveredInternal(Span<engine::GameObject* const> value);
        Array<engine::PPtr<engine::GameObject>>& hovered() { return hovered_; }

        HX_PROPERTY() engine::GameObject* pointerEnter() const { return pointerEnter_.Get(); }
        void SetPointerEnter(engine::GameObject* value) { pointerEnter_ = engine::PPtr<engine::GameObject>(value); }
        HX_PROPERTY(ReadOnly) engine::GameObject* lastPress() const { return lastPress_.Get(); }
        HX_PROPERTY() engine::GameObject* rawPointerPress() const { return rawPointerPress_.Get(); }
        void SetRawPointerPress(engine::GameObject* value) { rawPointerPress_ = engine::PPtr<engine::GameObject>(value); }
        HX_PROPERTY() engine::GameObject* pointerDrag() const { return pointerDrag_.Get(); }
        void SetPointerDrag(engine::GameObject* value) { pointerDrag_ = engine::PPtr<engine::GameObject>(value); }
        HX_PROPERTY() engine::GameObject* pointerClick() const { return pointerClick_.Get(); }
        void SetPointerClick(engine::GameObject* value) { pointerClick_ = engine::PPtr<engine::GameObject>(value); }
        // Setting the press keeps the last one in lastPress.
        HX_PROPERTY() engine::GameObject* pointerPress() const { return pointerPress_.Get(); }
        void SetPointerPress(engine::GameObject* value);
        HX_PROPERTY(Internal, Name = "pointerCurrentRaycastInternal") RaycastResultData CurrentRaycastData() const { return RaycastResultData::From(pointerCurrentRaycast); }
        void SetCurrentRaycastData(RaycastResultData value) { pointerCurrentRaycast = value.ToResult(); }
        HX_PROPERTY(Internal, Name = "pointerPressRaycastInternal") RaycastResultData PressRaycastData() const { return RaycastResultData::From(pointerPressRaycast); }
        void SetPressRaycastData(RaycastResultData value) { pointerPressRaycast = value.ToResult(); }

        HX_PROPERTY() bool eligibleForClick() const { return eligibleForClick_; }
        void SetEligibleForClick(bool value) { eligibleForClick_ = value; }
        HX_PROPERTY() i32 displayIndex() const { return displayIndex_; }
        void SetDisplayIndex(i32 value) { displayIndex_ = value; }
        HX_PROPERTY() i32 pointerId() const { return pointerId_; }
        void SetPointerId(i32 value) { pointerId_ = value; }
        HX_PROPERTY() Vector2 position() const { return position_; }
        void SetPosition(Vector2 value) { position_ = value; }
        HX_PROPERTY() Vector2 delta() const { return delta_; }
        void SetDelta(Vector2 value) { delta_ = value; }
        HX_PROPERTY() Vector2 pressPosition() const { return pressPosition_; }
        void SetPressPosition(Vector2 value) { pressPosition_ = value; }
        HX_PROPERTY(Attributes = "[System.Obsolete(\"Use either pointerCurrentRaycast.worldPosition or pointerPressRaycast.worldPosition\")]")
        Vector3 worldPosition() const { return worldPosition_; }
        void SetWorldPosition(Vector3 value) { worldPosition_ = value; }
        HX_PROPERTY(Attributes = "[System.Obsolete(\"Use either pointerCurrentRaycast.worldNormal or pointerPressRaycast.worldNormal\")]")
        Vector3 worldNormal() const { return worldNormal_; }
        void SetWorldNormal(Vector3 value) { worldNormal_ = value; }
        HX_PROPERTY() f32 clickTime() const { return clickTime_; }
        void SetClickTime(f32 value) { clickTime_ = value; }
        HX_PROPERTY() i32 clickCount() const { return clickCount_; }
        void SetClickCount(i32 value) { clickCount_ = value; }
        HX_PROPERTY() Vector2 scrollDelta() const { return scrollDelta_; }
        void SetScrollDelta(Vector2 value) { scrollDelta_ = value; }
        HX_PROPERTY() bool useDragThreshold() const { return useDragThreshold_; }
        void SetUseDragThreshold(bool value) { useDragThreshold_ = value; }
        HX_PROPERTY() bool dragging() const { return dragging_; }
        void SetDragging(bool value) { dragging_ = value; }
        HX_PROPERTY() InputButton button() const { return button_; }
        void SetButton(InputButton value) { button_ = value; }
        HX_PROPERTY() f32 pressure() const { return pressure_; }
        void SetPressure(f32 value) { pressure_ = value; }
        HX_PROPERTY() f32 tangentialPressure() const { return tangentialPressure_; }
        void SetTangentialPressure(f32 value) { tangentialPressure_ = value; }
        HX_PROPERTY() f32 altitudeAngle() const { return altitudeAngle_; }
        void SetAltitudeAngle(f32 value) { altitudeAngle_ = value; }
        HX_PROPERTY() f32 azimuthAngle() const { return azimuthAngle_; }
        void SetAzimuthAngle(f32 value) { azimuthAngle_ = value; }
        HX_PROPERTY() f32 twist() const { return twist_; }
        void SetTwist(f32 value) { twist_ = value; }
        HX_PROPERTY() Vector2 tilt() const { return tilt_; }
        void SetTilt(Vector2 value) { tilt_ = value; }
        HX_PROPERTY() ui::gui::PenStatus penStatus() const { return penStatus_; }
        void SetPenStatus(ui::gui::PenStatus value) { penStatus_ = value; }
        HX_PROPERTY() Vector2 radius() const { return radius_; }
        void SetRadius(Vector2 value) { radius_ = value; }
        HX_PROPERTY() Vector2 radiusVariance() const { return radiusVariance_; }
        void SetRadiusVariance(Vector2 value) { radiusVariance_ = value; }
        HX_PROPERTY() bool fullyExited() const { return fullyExited_; }
        void SetFullyExited(bool value) { fullyExited_ = value; }
        HX_PROPERTY() bool reentered() const { return reentered_; }
        void SetReentered(bool value) { reentered_ = value; }
        // The cameras of the raycasters that found the entered and the pressed object.
        HX_PROPERTY(ReadOnly) engine::Camera* enterEventCamera() const;
        HX_PROPERTY(ReadOnly) engine::Camera* pressEventCamera() const;
        HX_FUNCTION() bool IsPointerMoving() const { return delta_.x * delta_.x + delta_.y * delta_.y > 0; }
        HX_FUNCTION() bool IsScrolling() const { return scrollDelta_.x * scrollDelta_.x + scrollDelta_.y * scrollDelta_.y > 0; }
        HX_FUNCTION(Internal) String Describe() const;

        scripting::TypeId HelixType() const override { return scripting::TypeOf<PointerEventData>(); }

        RaycastResult pointerCurrentRaycast;
        RaycastResult pointerPressRaycast;

    private:
        Array<engine::PPtr<engine::GameObject>> hovered_;
        engine::PPtr<engine::GameObject> pointerEnter_, lastPress_, rawPointerPress_, pointerDrag_, pointerClick_, pointerPress_;
        bool eligibleForClick_ = false, useDragThreshold_ = true, dragging_ = false, fullyExited_ = false, reentered_ = false;
        i32 displayIndex_ = 0, pointerId_ = -1, clickCount_ = 0;
        Vector2 position_ = Vector2(0, 0), delta_ = Vector2(0, 0), pressPosition_ = Vector2(0, 0), scrollDelta_ = Vector2(0, 0);
        Vector3 worldPosition_ = Vector3(0, 0, 0), worldNormal_ = Vector3(0, 0, 0);
        f32 clickTime_ = 0, pressure_ = 1, tangentialPressure_ = 0, altitudeAngle_ = 1.5707964f, azimuthAngle_ = 0, twist_ = 0;
        Vector2 tilt_ = Vector2(0, 0), radius_ = Vector2(0, 0), radiusVariance_ = Vector2(0, 0);
        ui::gui::PenStatus penStatus_ = ui::gui::PenStatus::None;
        InputButton button_ = InputButton::Left;
    };
}

HX_CANVASUI_ROOT_HANDLES(helix::canvasui::AbstractEventData)
