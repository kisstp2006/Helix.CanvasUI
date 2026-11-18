#pragma once

#include <Helix/CanvasUI/Events/EventData.h>
#include <Helix/Engine/Event/Event.h>

// The handler interfaces: a component implementing one hears that event when the event system sends it to its
// GameObject. ExecuteEvents sends an event to the components of a GameObject (or the first GameObject up the hierarchy
// that handles it); the components of C++ classes answer in C++, those of C# classes through the C# side.
namespace helix::canvasui
{
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IEventSystemHandler
    {
    public:
        virtual ~IEventSystemHandler() = default;
    };

    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IPointerEnterHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnPointerEnter(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IPointerExitHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnPointerExit(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IPointerMoveHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnPointerMove(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IPointerDownHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnPointerDown(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IPointerUpHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnPointerUp(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IPointerClickHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnPointerClick(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IInitializePotentialDragHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnInitializePotentialDrag(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IBeginDragHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnBeginDrag(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IDragHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnDrag(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IEndDragHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnEndDrag(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IDropHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnDrop(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IScrollHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnScroll(PointerEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IUpdateSelectedHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnUpdateSelected(BaseEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class ISelectHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnSelect(BaseEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IDeselectHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnDeselect(BaseEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class IMoveHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnMove(AxisEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class ISubmitHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnSubmit(BaseEventData* eventData) = 0;
    };
    HX_INTERFACE(Namespace = "HelixEngine.UIEvents")
    class ICancelHandler : public IEventSystemHandler
    {
    public:
        HX_FUNCTION() virtual void OnCancel(BaseEventData* eventData) = 0;
    };

    // The events by kind (the order of the reference's ExecuteEvents handlers).
    HX_ENUM(Namespace = "HelixEngine.UIEvents", Internal)
    enum class EventKind : i32
    {
        PointerEnter = 0,
        PointerExit,
        PointerMove,
        PointerDown,
        PointerUp,
        PointerClick,
        InitializePotentialDrag,
        BeginDrag,
        Drag,
        EndDrag,
        Drop,
        Scroll,
        UpdateSelected,
        Select,
        Deselect,
        Move,
        Submit,
        Cancel,
        Count,
    };

    // A component of C++ that handles events: the kinds it handles and the call.
    class INativeEventHandler
    {
    public:
        virtual ~INativeEventHandler() = default;
        virtual bool Handles(EventKind kind) const = 0;
        virtual void HandleEvent(EventKind kind, BaseEventData* data) = 0;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents", Static)
    class ExecuteEvents
    {
    public:
        // The event to every handler of it on the GameObject (enabled ones); whether any heard it.
        HX_FUNCTION(Internal) static bool ExecuteInternal(engine::GameObject* target, BaseEventData* eventData, EventKind kind);
        // To the first GameObject from root up that handles it; that GameObject.
        HX_FUNCTION(Internal) static engine::GameObject* ExecuteHierarchyInternal(engine::GameObject* root, BaseEventData* eventData, EventKind kind);
        HX_FUNCTION(Internal) static bool CanHandleEventInternal(engine::GameObject* go, EventKind kind);
        HX_FUNCTION(Internal) static engine::GameObject* GetEventHandlerInternal(engine::GameObject* root, EventKind kind);
        // C#'s components: the C# side handles an event (the answer: whether the component took it) through SetHandled.
        HX_EVENT(Internal) static engine::Event<engine::Component*, i32, BaseEventData*>& managedExecute();
        HX_FUNCTION(Internal) static void SetHandled(bool value);
    };
}
