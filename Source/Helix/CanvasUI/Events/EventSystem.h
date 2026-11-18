#pragma once

#include <Helix/CanvasUI/Core/UIBehaviour.h>
#include <Helix/CanvasUI/Events/EventHandlers.h>
#include <Helix/CanvasUI/Rendering/Graphic.h>
#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Events/HelixEvent.h>
#include <Helix/Engine/Input/Touch.h>

// The event system: one EventSystem (current) drives its input module each frame. The module reads the input
// (BaseInput: the mouse, touches, the navigation keys), raycasts through every raycaster (GraphicRaycaster on the
// canvases, in every render mode and turned in 3D), and sends the events to the objects hit: enter and exit, down,
// up and click, drags and drops, scrolls, and the selection's move, submit and cancel.
namespace helix::canvasui
{
    HX_ENUM(Namespace = "HelixEngine.UIEvents")
    enum class NavigationDeviceType : i32
    {
        Unknown = 0,
        Keyboard = 1,
        NonKeyboard = 2,
    };

    HX_ENUM(Namespace = "HelixEngine.UIEvents")
    enum class EventTriggerType : i32
    {
        PointerEnter = 0,
        PointerExit = 1,
        PointerDown = 2,
        PointerUp = 3,
        PointerClick = 4,
        Drag = 5,
        Drop = 6,
        Scroll = 7,
        UpdateSelected = 8,
        Select = 9,
        Deselect = 10,
        Move = 11,
        InitializePotentialDrag = 12,
        BeginDrag = 13,
        EndDrag = 14,
        Submit = 15,
        Cancel = 16,
    };

    class InputModule;

    // The input the modules read: the frame's mouse, touches, input method and navigation (Horizontal, Vertical,
    // Submit and Cancel as the default input settings name them).
    HX_CLASS(Namespace = "HelixEngine.UIEvents")
    class BaseInput : public UIBehaviour
    {
    public:
        HX_PROPERTY(ReadOnly) virtual String compositionString() const;
        HX_PROPERTY() virtual engine::IMECompositionMode imeCompositionMode() const;
        virtual void SetImeCompositionMode(engine::IMECompositionMode value);
        HX_PROPERTY() virtual Vector2 compositionCursorPos() const;
        virtual void SetCompositionCursorPos(Vector2 value);
        HX_PROPERTY(ReadOnly) virtual bool mousePresent() const;
        HX_PROPERTY(ReadOnly) virtual Vector2 mousePosition() const;
        HX_PROPERTY(ReadOnly) virtual Vector2 mouseScrollDelta() const;
        HX_PROPERTY(ReadOnly) virtual f32 mouseScrollDeltaPerTick() const { return 1; }
        HX_PROPERTY(ReadOnly) virtual bool touchSupported() const;
        HX_PROPERTY(ReadOnly) virtual i32 touchCount() const;
        HX_FUNCTION() virtual engine::Touch GetTouch(i32 index);
        HX_FUNCTION() virtual bool GetMouseButtonDown(i32 button);
        HX_FUNCTION() virtual bool GetMouseButtonUp(i32 button);
        HX_FUNCTION() virtual bool GetMouseButton(i32 button);
        HX_FUNCTION() virtual f32 GetAxisRaw(StringView axisName);
        HX_FUNCTION() virtual bool GetButtonDown(StringView buttonName);
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents", Abstract)
    class BaseRaycaster : public UIBehaviour
    {
    public:
        // Hits appended to Results() for the pointer (sorted by the event system after every raycaster ran); a raycaster
        // of a C# class raycasts on the C# side (managedRaycast).
        virtual void RaycastInternal(PointerEventData* eventData);
        // For C#: this raycaster's hits (the C# override's base), and a hit a C# raycaster found.
        HX_FUNCTION(Internal) Array<RaycastResultData> RaycastNative(PointerEventData* eventData);
        HX_FUNCTION(Internal) static void AppendResultInternal(RaycastResultData result);
        HX_EVENT(Internal) static engine::Event<BaseRaycaster*, PointerEventData*>& managedRaycast();
        HX_PROPERTY(ReadOnly) virtual engine::Camera* eventCamera() const = 0;
        HX_PROPERTY(ReadOnly, Attributes = "[System.Obsolete(\"Please use sortOrderPriority and renderOrderPriority\", false)]") virtual i32 priority() const { return 0; }
        HX_PROPERTY(ReadOnly) virtual i32 sortOrderPriority() const { return 0x80000000; }
        HX_PROPERTY(ReadOnly) virtual i32 renderOrderPriority() const { return 0x80000000; }
        // The raycaster of the root canvas above (itself without one).
        HX_PROPERTY(ReadOnly) BaseRaycaster* rootRaycaster();
        HX_FUNCTION(Internal) String Describe() const;

        void OnEnable() override;
        void OnDisable() override;
        void OnCanvasHierarchyChanged() override { rootRaycaster_ = engine::PPtr<engine::Object>(); }
        void OnTransformParentChanged() override { rootRaycaster_ = engine::PPtr<engine::Object>(); }

        // The results of the current raycast (what RaycastInternal appends to).
        static Array<RaycastResult>& Results();

    private:
        engine::PPtr<engine::Object> rootRaycaster_;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents", Static)
    class RaycasterManager
    {
    public:
        HX_FUNCTION(Internal) static Array<BaseRaycaster*> GetRaycastersInternal();
        static void Add(BaseRaycaster* raycaster);
        static void Remove(BaseRaycaster* raycaster);
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents")
    class EventSystem : public UIBehaviour
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        ~EventSystem() override;

        HX_PROPERTY() static EventSystem* current();
        static void SetCurrent(EventSystem* value);
        HX_PROPERTY() bool sendNavigationEvents() const { return sendNavigationEvents_; }
        void SetSendNavigationEvents(bool value) { sendNavigationEvents_ = value; }
        HX_PROPERTY() i32 pixelDragThreshold() const { return dragThreshold_; }
        void SetPixelDragThreshold(i32 value) { dragThreshold_ = value; }
        HX_PROPERTY(ReadOnly) InputModule* currentInputModule() const;
        HX_PROPERTY() engine::GameObject* firstSelectedGameObject() const { return firstSelected_.Get(); }
        void SetFirstSelectedGameObject(engine::GameObject* value) { firstSelected_ = engine::PPtr<engine::GameObject>(value); }
        HX_PROPERTY(ReadOnly) engine::GameObject* currentSelectedGameObject() const { return currentSelected_.Get(); }
        HX_PROPERTY(ReadOnly, Attributes = "[System.Obsolete(\"lastSelectedGameObject is no longer supported\")]") engine::GameObject* lastSelectedGameObject() const
        {
            return nullptr;
        }
        HX_PROPERTY(ReadOnly) bool isFocused() const { return hasFocus_; }
        HX_PROPERTY(ReadOnly) bool alreadySelecting() const { return selectionGuard_; }
        // The input modules of its GameObject, found again.
        HX_FUNCTION() void UpdateModules();
        HX_FUNCTION() void SetSelectedGameObject(engine::GameObject* selected, BaseEventData* pointer);
        HX_FUNCTION() void SetSelectedGameObject(engine::GameObject* selected);
        // Every raycaster's hits for the pointer, sorted (the topmost first).
        HX_FUNCTION(Internal) Array<RaycastResultData> RaycastAllInternal(PointerEventData* eventData);
        void RaycastAll(PointerEventData* eventData, Array<RaycastResult>& results);
        HX_FUNCTION() bool IsPointerOverGameObject();
        HX_FUNCTION() bool IsPointerOverGameObject(i32 pointerId);
        HX_FUNCTION(Internal) String Describe() const;

        HX_FUNCTION(Protected) void OnEnable() override;
        HX_FUNCTION(Protected) void OnDisable() override;
        HX_FUNCTION(Protected) void Start() override;
        HX_FUNCTION(Protected) virtual void Update();
        HX_FUNCTION(Protected) virtual void OnApplicationFocus(bool hasFocus) { hasFocus_ = hasFocus; }

        // The base event data the selection changes are sent with.
        BaseEventData* DummyData();

    private:
        void ChangeEventModule(InputModule* module);
        void TickModules();

        Array<engine::PPtr<engine::Object>> modules_;
        engine::PPtr<engine::Object> currentModule_;
        HX_FIELD(SerializeField, Name = "m_FirstSelected") engine::PPtr<engine::GameObject> firstSelected_;
        HX_FIELD(SerializeField, Name = "m_sendNavigationEvents") bool sendNavigationEvents_ = true;
        HX_FIELD(SerializeField, Name = "m_DragThreshold") i32 dragThreshold_ = 10;
        engine::PPtr<engine::GameObject> currentSelected_;
        bool hasFocus_ = true, selectionGuard_ = false;
        BaseEventData* dummyData_ = nullptr;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents", Abstract, RequireComponent = "canvasui::EventSystem")
    class InputModule : public UIBehaviour
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        ~InputModule() override;

        HX_PROPERTY(ReadOnly) BaseInput* input();
        HX_PROPERTY() BaseInput* inputOverride() const { return static_cast<BaseInput*>(inputOverride_.Get()); }
        void SetInputOverride(BaseInput* value) { inputOverride_ = engine::PPtr<engine::Object>(value); }
        HX_PROPERTY(ReadOnly, Protected) EventSystem* eventSystem() const;
        HX_PROPERTY(Protected) bool sendPointerHoverToParent() const { return sendPointerHoverToParent_; }
        void SetSendPointerHoverToParent(bool value) { sendPointerHoverToParent_ = value; }

        HX_FUNCTION() virtual void Process() = 0;
        HX_FUNCTION() virtual bool IsPointerOverGameObject(i32 pointerId)
        {
            (void)pointerId;
            return false;
        }
        HX_FUNCTION() virtual bool ShouldActivateModule() { return isActiveAndEnabled(); }
        HX_FUNCTION() virtual void DeactivateModule() {}
        HX_FUNCTION() virtual void ActivateModule() {}
        HX_FUNCTION() virtual void UpdateModule() {}
        HX_FUNCTION() virtual bool IsModuleSupported() { return true; }
        HX_FUNCTION() virtual i32 ConvertUIToolkitPointerId(PointerEventData* sourcePointerData);
        HX_FUNCTION() virtual Vector2 ConvertPointerEventScrollDeltaToTicks(Vector2 scrollDelta);
        HX_FUNCTION() virtual NavigationDeviceType GetNavigationEventDeviceType(BaseEventData* eventData)
        {
            (void)eventData;
            return NavigationDeviceType::Unknown;
        }

        // The topmost hit (the first with a GameObject).
        static RaycastResult FindFirstRaycast(Span<const RaycastResult> candidates);
        HX_FUNCTION(Internal, Name = "FindFirstRaycastInternal") static RaycastResultData FindFirstRaycastData(Span<const RaycastResultData> candidates);
        HX_FUNCTION(Protected) static MoveDirection DetermineMoveDirection(f32 x, f32 y);
        HX_FUNCTION(Protected) static MoveDirection DetermineMoveDirection(f32 x, f32 y, f32 deadZone);
        HX_FUNCTION(Protected) static engine::GameObject* FindCommonRoot(engine::GameObject* g1, engine::GameObject* g2);
        // Exit events to what the pointer left, enter events to what it entered (up to their common root).
        HX_FUNCTION(Protected) void HandlePointerExitAndEnter(PointerEventData* currentPointerData, engine::GameObject* newEnterTarget);
        HX_FUNCTION(Protected) virtual AxisEventData* GetAxisEventData(f32 x, f32 y, f32 moveDeadZone);
        HX_FUNCTION(Protected) virtual BaseEventData* GetBaseEventData();

        void OnEnable() override;
        void OnDisable() override;

    protected:
        Array<RaycastResult> raycastResultCache_;

    private:
        engine::PPtr<engine::Object> inputOverride_, defaultInput_;
        bool sendPointerHoverToParent_ = true;
        AxisEventData* axisEventData_ = nullptr;
        BaseEventData* baseEventData_ = nullptr;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents", Abstract)
    class PointerInputModule : public InputModule
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        static constexpr i32 kMouseLeftId = -1;
        static constexpr i32 kMouseRightId = -2;
        static constexpr i32 kMouseMiddleId = -3;
        static constexpr i32 kFakeTouchesId = -4;

        ~PointerInputModule() override;

        // A pointer's data (made when asked to); whether it was made now.
        HX_FUNCTION(Protected, Out = "data") bool GetPointerData(i32 id, PointerEventData*& data, bool create);
        HX_FUNCTION(Protected) void RemovePointerData(PointerEventData* data);
        HX_FUNCTION(Protected, Out = "pressed,released") PointerEventData* GetTouchPointerEventData(engine::Touch input, bool& pressed, bool& released);
        HX_FUNCTION(Protected) void CopyFromTo(PointerEventData* from, PointerEventData* to);
        HX_FUNCTION(Protected) PointerEventData::FramePressState StateForMouseButton(i32 buttonId);
        HX_FUNCTION(Protected) PointerEventData* GetLastPointerEventData(i32 id);
        HX_FUNCTION(Protected) virtual void ProcessMove(PointerEventData* pointerEvent);
        HX_FUNCTION(Protected) virtual void ProcessDrag(PointerEventData* pointerEvent);
        HX_FUNCTION() bool IsPointerOverGameObject(i32 pointerId) override;
        HX_FUNCTION(Protected) void ClearSelection();
        HX_FUNCTION(Protected) void DeselectIfSelectionChanged(engine::GameObject* currentOverGo, BaseEventData* pointerEvent);
        HX_FUNCTION(Internal) String Describe() const;

        // The mouse's three buttons this frame (the left button's data moves and raycasts; the others copy it).
        struct ButtonFrame
        {
            PointerEventData* data = nullptr;
            PointerEventData::FramePressState state = PointerEventData::FramePressState::NotChanged;
            bool PressedThisFrame() const
            {
                return state == PointerEventData::FramePressState::Pressed || state == PointerEventData::FramePressState::PressedAndReleased;
            }
            bool ReleasedThisFrame() const
            {
                return state == PointerEventData::FramePressState::Released || state == PointerEventData::FramePressState::PressedAndReleased;
            }
        };
        void MouseFrame(i32 id, ButtonFrame (&buttons)[3]);
        HX_FUNCTION(Internal, Out = "data,state") void MouseButtonInternal(i32 id, i32 button, PointerEventData*& data, i32& state);

    protected:
        bool PointerDataOf(i32 id, PointerEventData*& data, bool create);

        struct Pointer
        {
            i32 id = 0;
            PointerEventData* data = nullptr;
        };
        Array<Pointer> pointerData_;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents")
    class StandaloneInputModule : public PointerInputModule
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        HX_ENUM(Attributes = "[System.Obsolete(\"Mode is no longer needed on input module as it handles both mouse and keyboard simultaneously.\", false)]")
        enum class InputMode : i32
        {
            Mouse = 0,
            Buttons = 1,
        };

        HX_PROPERTY(ReadOnly, Attributes = "[System.Obsolete(\"Mode is no longer needed on input module as it handles both mouse and keyboard simultaneously.\", false)]")
        InputMode inputMode() const { return InputMode::Mouse; }
        HX_PROPERTY(Attributes = "[System.Obsolete(\"allowActivationOnMobileDevice has been deprecated. Use forceModuleActive instead (UnityUpgradable) -> forceModuleActive\")]")
        bool allowActivationOnMobileDevice() const { return forceModuleActive_; }
        void SetAllowActivationOnMobileDevice(bool value) { forceModuleActive_ = value; }
        HX_PROPERTY(Attributes = "[System.Obsolete(\"forceModuleActive has been deprecated. There is no need to force the module awake as StandaloneInputModule works for all platforms\")]")
        bool forceModuleActive() const { return forceModuleActive_; }
        void SetForceModuleActive(bool value) { forceModuleActive_ = value; }
        HX_PROPERTY() f32 inputActionsPerSecond() const { return inputActionsPerSecond_; }
        void SetInputActionsPerSecond(f32 value) { inputActionsPerSecond_ = value; }
        HX_PROPERTY() f32 repeatDelay() const { return repeatDelay_; }
        void SetRepeatDelay(f32 value) { repeatDelay_ = value; }
        HX_PROPERTY() StringView horizontalAxis() const { return horizontalAxis_; }
        void SetHorizontalAxis(StringView value) { horizontalAxis_ = String(value); }
        HX_PROPERTY() StringView verticalAxis() const { return verticalAxis_; }
        void SetVerticalAxis(StringView value) { verticalAxis_ = String(value); }
        HX_PROPERTY() StringView submitButton() const { return submitButton_; }
        void SetSubmitButton(StringView value) { submitButton_ = String(value); }
        HX_PROPERTY() StringView cancelButton() const { return cancelButton_; }
        void SetCancelButton(StringView value) { cancelButton_ = String(value); }

        HX_FUNCTION() void UpdateModule() override;
        HX_FUNCTION() bool ShouldActivateModule() override;
        HX_FUNCTION() void ActivateModule() override;
        HX_FUNCTION() void DeactivateModule() override { ClearSelection(); }
        HX_FUNCTION() void Process() override;
        HX_FUNCTION(Protected) void ProcessTouchPress(PointerEventData* pointerEvent, bool pressed, bool released);
        HX_FUNCTION(Protected) bool SendSubmitEventToSelectedObject();
        HX_FUNCTION(Protected) bool SendMoveEventToSelectedObject();
        HX_FUNCTION(Protected) void ProcessMouseEvent();
        HX_FUNCTION(Protected) void ProcessMouseEvent(i32 id);
        HX_FUNCTION(Protected) bool SendUpdateEventToSelectedObject();
        HX_FUNCTION(Protected) engine::GameObject* GetCurrentFocusedGameObject() const { return currentFocusedGameObject_.Get(); }
        HX_FUNCTION(Protected, Attributes = "[System.Obsolete(\"This method is no longer checked, overriding it with return true does nothing!\")]") virtual bool ForceAutoSelect()
        {
            return false;
        }
        // The press and release of one mouse button's data (MouseButtonEventData in C#).
        void ProcessMousePress(const ButtonFrame& data);
        HX_FUNCTION(Internal) void ProcessMousePressInternal(PointerEventData* data, i32 state);

    private:
        bool ProcessTouchEvents();
        Vector2 RawNavigation();
        bool ShouldIgnoreEventsOnNoFocus() const;
        void ReleaseMouse(PointerEventData* pointerEvent, engine::GameObject* currentOverGo);

        f32 prevActionTime_ = 0;
        Vector2 lastMoveVector_ = Vector2(0, 0);
        i32 consecutiveMoveCount_ = 0;
        Vector2 lastMousePosition_ = Vector2(0, 0), mousePosition_ = Vector2(0, 0);
        engine::PPtr<engine::GameObject> currentFocusedGameObject_;
        PointerEventData* inputPointerEvent_ = nullptr;
        HX_FIELD(SerializeField, Name = "m_HorizontalAxis") String horizontalAxis_ = String("Horizontal");
        HX_FIELD(SerializeField, Name = "m_VerticalAxis") String verticalAxis_ = String("Vertical");
        HX_FIELD(SerializeField, Name = "m_SubmitButton") String submitButton_ = String("Submit");
        HX_FIELD(SerializeField, Name = "m_CancelButton") String cancelButton_ = String("Cancel");
        HX_FIELD(SerializeField, Name = "m_InputActionsPerSecond") f32 inputActionsPerSecond_ = 10;
        HX_FIELD(SerializeField, Name = "m_RepeatDelay") f32 repeatDelay_ = 0.5f;
        HX_FIELD(SerializeField, Name = "m_ForceModuleActive") bool forceModuleActive_ = false;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents", Attributes = "[System.Obsolete(\"TouchInputModule is no longer required as Touch input is now handled in StandaloneInputModule.\")]")
    class TouchInputModule : public PointerInputModule
    {
    public:
        HX_PROPERTY(Attributes = "[System.Obsolete(\"allowActivationOnStandalone has been deprecated. Use forceModuleActive instead (UnityUpgradable) -> forceModuleActive\")]")
        bool allowActivationOnStandalone() const { return forceModuleActive_; }
        void SetAllowActivationOnStandalone(bool value) { forceModuleActive_ = value; }
        HX_PROPERTY() bool forceModuleActive() const { return forceModuleActive_; }
        void SetForceModuleActive(bool value) { forceModuleActive_ = value; }
        HX_FUNCTION() void UpdateModule() override;
        HX_FUNCTION() bool IsModuleSupported() override;
        HX_FUNCTION() bool ShouldActivateModule() override;
        HX_FUNCTION() void Process() override;
        HX_FUNCTION(Protected) void ProcessTouchPress(PointerEventData* pointerEvent, bool pressed, bool released);
        HX_FUNCTION() void DeactivateModule() override { ClearSelection(); }

    private:
        Vector2 lastMousePosition_ = Vector2(0, 0), mousePosition_ = Vector2(0, 0);
        HX_FIELD(SerializeField, Name = "m_ForceModuleActive") bool forceModuleActive_ = false;
    };

    // A trigger's entry: an event type and the callback that hears it.
    HX_STRUCT(SerializedOnly)
    struct EventTriggerEntry
    {
        static constexpr StringView kTypeName = "Entry";
        EventTriggerType eventID = EventTriggerType::PointerClick;
        engine::HelixEvent<BaseEventData*> callback;

        template <class V>
        void Transfer(V& v)
        {
            i32 id = i32(eventID);
            v.Field("eventID", id);
            eventID = EventTriggerType(id);
            v.Field("callback", callback);
        }
    };

    HX_STRUCT(SerializedOnly)
    struct EventTriggerEntries
    {
        static constexpr StringView kTypeName = "vector";
        Array<EventTriggerEntry> entries;

        template <class V>
        void Transfer(V& v)
        {
            v.Field("Array", entries);
        }
    };

    // Every event to the callbacks of its entries (C#'s Entry and TriggerEvent over the native ones).
    HX_CLASS(Namespace = "HelixEngine.UIEvents")
    class EventTrigger : public UIBehaviour, public INativeEventHandler
    {
    public:
        EventTrigger();
        bool Handles(EventKind kind) const override;
        void HandleEvent(EventKind kind, BaseEventData* data) override;
        INativeEventHandler* AsEventHandler() override { return this; }
        HX_FUNCTION(Internal) i32 EntryCount() const { return i32(triggers_.entries.Size()); }
        HX_FUNCTION(Internal) EventTriggerType EntryType(i32 index) const;
        HX_FUNCTION(Internal) void SetEntryType(i32 index, EventTriggerType type);
        HX_FUNCTION(Internal) void AddEntry(EventTriggerType type);
        HX_FUNCTION(Internal) void RemoveEntry(i32 index);
        HX_FUNCTION(Internal) void ClearEntries() { triggers_.entries.Clear(); }
        engine::HelixEventStorage* EntryStorage(i32 index);
        HX_FUNCTION(Internal) void RaiseEntry(i32 index, BaseEventData* eventData);
        // The C# side raises its listeners: (entry index, event data).
        HX_EVENT(Internal) engine::Event<i32, BaseEventData*>& entryRaised() { return entryRaised_; }

        HX_FUNCTION() virtual void OnPointerEnter(PointerEventData* eventData) { Run(EventTriggerType::PointerEnter, eventData); }
        HX_FUNCTION() virtual void OnPointerExit(PointerEventData* eventData) { Run(EventTriggerType::PointerExit, eventData); }
        HX_FUNCTION() virtual void OnDrag(PointerEventData* eventData) { Run(EventTriggerType::Drag, eventData); }
        HX_FUNCTION() virtual void OnDrop(PointerEventData* eventData) { Run(EventTriggerType::Drop, eventData); }
        HX_FUNCTION() virtual void OnPointerDown(PointerEventData* eventData) { Run(EventTriggerType::PointerDown, eventData); }
        HX_FUNCTION() virtual void OnPointerUp(PointerEventData* eventData) { Run(EventTriggerType::PointerUp, eventData); }
        HX_FUNCTION() virtual void OnPointerClick(PointerEventData* eventData) { Run(EventTriggerType::PointerClick, eventData); }
        HX_FUNCTION() virtual void OnSelect(BaseEventData* eventData) { Run(EventTriggerType::Select, eventData); }
        HX_FUNCTION() virtual void OnDeselect(BaseEventData* eventData) { Run(EventTriggerType::Deselect, eventData); }
        HX_FUNCTION() virtual void OnScroll(PointerEventData* eventData) { Run(EventTriggerType::Scroll, eventData); }
        HX_FUNCTION() virtual void OnMove(AxisEventData* eventData) { Run(EventTriggerType::Move, eventData); }
        HX_FUNCTION() virtual void OnUpdateSelected(BaseEventData* eventData) { Run(EventTriggerType::UpdateSelected, eventData); }
        HX_FUNCTION() virtual void OnInitializePotentialDrag(PointerEventData* eventData) { Run(EventTriggerType::InitializePotentialDrag, eventData); }
        HX_FUNCTION() virtual void OnBeginDrag(PointerEventData* eventData) { Run(EventTriggerType::BeginDrag, eventData); }
        HX_FUNCTION() virtual void OnEndDrag(PointerEventData* eventData) { Run(EventTriggerType::EndDrag, eventData); }
        HX_FUNCTION() virtual void OnSubmit(BaseEventData* eventData) { Run(EventTriggerType::Submit, eventData); }
        HX_FUNCTION() virtual void OnCancel(BaseEventData* eventData) { Run(EventTriggerType::Cancel, eventData); }

    private:
        void Run(EventTriggerType id, BaseEventData* eventData);

        HX_FIELD(SerializeField, Name = "m_Delegates") EventTriggerEntries triggers_;
        engine::Event<i32, BaseEventData*> entryRaised_;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::Canvas")
    class GraphicRaycaster : public BaseRaycaster
    {
    public:
        HX_ENUM()
        enum class BlockingObjects : i32
        {
            None = 0,
            TwoD = 1,
            ThreeD = 2,
            All = 3,
        };
        static constexpr i32 kNoEventMaskSet = -1;

        HX_PROPERTY() BlockingObjects blockingObjects() const { return blockingObjects_; }
        void SetBlockingObjects(BlockingObjects value) { blockingObjects_ = value; }
        HX_PROPERTY(Internal, Name = "blockingMaskValue") i32 blockingMask() const { return blockingMask_; }
        void SetBlockingMask(i32 value) { blockingMask_ = value; }
        HX_PROPERTY() bool ignoreReversedGraphics() const { return ignoreReversedGraphics_; }
        void SetIgnoreReversedGraphics(bool value) { ignoreReversedGraphics_ = value; }
        HX_PROPERTY(ReadOnly) engine::Camera* eventCamera() const override;
        HX_PROPERTY(ReadOnly) i32 sortOrderPriority() const override;
        HX_PROPERTY(ReadOnly) i32 renderOrderPriority() const override;
        void RaycastInternal(PointerEventData* eventData) override;

    private:
        engine::Canvas* CanvasOf() const;

        HX_FIELD(SerializeField, Name = "m_IgnoreReversedGraphics") bool ignoreReversedGraphics_ = true;
        HX_FIELD(SerializeField, Name = "m_BlockingObjects") BlockingObjects blockingObjects_ = BlockingObjects::None;
        HX_FIELD(SerializeField, Name = "m_BlockingMask") i32 blockingMask_ = -1;
    };

    // The raycasters of the physics scenes: their settings now, their hits with the physics step.
    HX_CLASS(Namespace = "HelixEngine.UIEvents", RequireComponent = "engine::Camera")
    class PhysicsRaycaster : public BaseRaycaster
    {
    public:
        static constexpr i32 kNoEventMaskSet = -1;

        HX_PROPERTY(ReadOnly) engine::Camera* eventCamera() const override;
        HX_PROPERTY(ReadOnly) virtual i32 depth() const;
        HX_PROPERTY(ReadOnly) i32 finalEventMask() const;
        HX_PROPERTY(Internal, Name = "eventMaskValue") i32 eventMask() const { return eventMask_; }
        void SetEventMask(i32 value) { eventMask_ = value; }
        HX_PROPERTY() i32 maxRayIntersections() const { return maxRayIntersections_; }
        void SetMaxRayIntersections(i32 value) { maxRayIntersections_ = value; }
        // The ray of the pointer through the event camera and the distance to its far plane; false outside its view.
        HX_FUNCTION(Protected) bool ComputeRayAndDistance(PointerEventData* eventData, Ray& ray, i32& eventDisplayIndex, f32& distanceToClipPlane);
        void RaycastInternal(PointerEventData* eventData) override;

    protected:
        HX_FIELD(SerializeField, Name = "m_EventMask") i32 eventMask_ = -1;
        HX_FIELD(SerializeField, Name = "m_MaxRayIntersections") i32 maxRayIntersections_ = 0;
        i32 lastMaxRayIntersections_ = 0;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents", RequireComponent = "engine::Camera")
    class Physics2DRaycaster : public PhysicsRaycaster
    {
    public:
        void RaycastInternal(PointerEventData* eventData) override;
    };
}
