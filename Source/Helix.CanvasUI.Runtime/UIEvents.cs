using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using HelixEngine.CanvasUI;
using HelixEngine.Events;
using HelixEngine.Internal;

namespace HelixEngine.UIEvents
{
    // The native event system's calls on components of C# classes: the handler interfaces they implement, and the
    // raycasts of raycasters written in C#.
    internal static class UIEventsHooks
    {
#pragma warning disable CA2255
        [ModuleInitializer]
#pragma warning restore CA2255
        internal static void Init() => HelixEngine.Internal.ScriptBridge.WhenBindingsLoaded(Start);

        // The hooks once the module's bindings reach the native side (the initializer runs as they load).
        static void Start()
        {
            ExecuteEvents.managedExecute += (c, kind, data) =>
            {
                bool query = kind < 0;
                int k = query ? -1 - kind : kind;
                bool handled = ExecuteEvents.ExecuteOnComponent(c, (EventKind)k, query ? null : data);
                ExecuteEvents.SetHandled(handled);
            };
            BaseRaycaster.managedRaycast += (raycaster, data) =>
            {
                var results = new List<RaycastResult>();
                raycaster.Raycast(data, results);
                foreach (RaycastResult r in results)
                    BaseRaycaster.AppendResultInternal(r.ToData());
            };
        }
    }

    public struct RaycastResult
    {
        GameObject m_GameObject;

        public BaseRaycaster module;
        public float distance;
        public float index;
        public int depth;
        public int sortingGroupID;
        public int sortingGroupOrder;
        public int sortingLayer;
        public int sortingOrder;
        public Vector3 origin;
        public Vector3 worldPosition;
        public Vector3 worldNormal;
        public Vector2 screenPosition;
        public int displayIndex;
        /// <summary>The UI view of a panel hit through the event system (none for the canvases' hits).</summary>
        public HelixEngine.UI.UIView document;
        /// <summary>The panel's element hit.</summary>
        public HelixEngine.UI.Element element;

        public GameObject gameObject
        {
            get => m_GameObject;
            set => m_GameObject = value;
        }

        public bool isValid => module != null && gameObject != null;

        public void Clear()
        {
            this = default;
        }

        public override string ToString()
        {
            if (!isValid)
                return "";
            return "Name: " + gameObject + "\n" + "module: " + module + "\n" + "distance: " + distance + "\n" + "index: " + index + "\n" + "depth: " + depth + "\n" +
                   "worldNormal: " + worldNormal + "\n" + "worldPosition: " + worldPosition + "\n" + "screenPosition: " + screenPosition + "\n" +
                   "module.sortOrderPriority: " + module.sortOrderPriority + "\n" + "module.renderOrderPriority: " + module.renderOrderPriority + "\n" +
                   "sortingLayer: " + sortingLayer + "\n" + "sortingOrder: " + sortingOrder;
        }

        internal static RaycastResult From(RaycastResultData d) => new RaycastResult
        {
            m_GameObject = Resources.EntityIdToObject(d.gameObject) as GameObject,
            module = Resources.EntityIdToObject(d.module) as BaseRaycaster,
            distance = d.distance,
            index = d.index,
            depth = d.depth,
            sortingGroupID = d.sortingGroupID,
            sortingGroupOrder = d.sortingGroupOrder,
            sortingLayer = d.sortingLayer,
            sortingOrder = d.sortingOrder,
            origin = d.origin,
            worldPosition = d.worldPosition,
            worldNormal = d.worldNormal,
            screenPosition = d.screenPosition,
            displayIndex = d.displayIndex,
        };

        internal RaycastResultData ToData() => new RaycastResultData
        {
            gameObject = m_GameObject != null ? m_GameObject.GetEntityId() : default,
            module = module != null ? module.GetEntityId() : default,
            distance = distance,
            index = index,
            depth = depth,
            sortingGroupID = sortingGroupID,
            sortingGroupOrder = sortingGroupOrder,
            sortingLayer = sortingLayer,
            sortingOrder = sortingOrder,
            origin = origin,
            worldPosition = worldPosition,
            worldNormal = worldNormal,
            screenPosition = screenPosition,
            displayIndex = displayIndex,
        };
    }

    public static partial class ExecuteEvents
    {
        public delegate void EventFunction<T1>(T1 handler, BaseEventData eventData);

        public static T ValidateEventData<T>(BaseEventData data) where T : class
        {
            if ((data as T) == null)
                throw new ArgumentException(string.Format("Invalid type: {0} passed to event expecting {1}", data.GetType(), typeof(T)));
            return data as T;
        }

        public static EventFunction<IPointerMoveHandler> pointerMoveHandler { get; } = (h, d) => h.OnPointerMove(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IPointerEnterHandler> pointerEnterHandler { get; } = (h, d) => h.OnPointerEnter(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IPointerExitHandler> pointerExitHandler { get; } = (h, d) => h.OnPointerExit(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IPointerDownHandler> pointerDownHandler { get; } = (h, d) => h.OnPointerDown(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IPointerUpHandler> pointerUpHandler { get; } = (h, d) => h.OnPointerUp(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IPointerClickHandler> pointerClickHandler { get; } = (h, d) => h.OnPointerClick(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IInitializePotentialDragHandler> initializePotentialDrag { get; } =
            (h, d) => h.OnInitializePotentialDrag(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IBeginDragHandler> beginDragHandler { get; } = (h, d) => h.OnBeginDrag(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IDragHandler> dragHandler { get; } = (h, d) => h.OnDrag(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IEndDragHandler> endDragHandler { get; } = (h, d) => h.OnEndDrag(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IDropHandler> dropHandler { get; } = (h, d) => h.OnDrop(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IScrollHandler> scrollHandler { get; } = (h, d) => h.OnScroll(ValidateEventData<PointerEventData>(d));
        public static EventFunction<IUpdateSelectedHandler> updateSelectedHandler { get; } = (h, d) => h.OnUpdateSelected(d);
        public static EventFunction<ISelectHandler> selectHandler { get; } = (h, d) => h.OnSelect(d);
        public static EventFunction<IDeselectHandler> deselectHandler { get; } = (h, d) => h.OnDeselect(d);
        public static EventFunction<IMoveHandler> moveHandler { get; } = (h, d) => h.OnMove(ValidateEventData<AxisEventData>(d));
        public static EventFunction<ISubmitHandler> submitHandler { get; } = (h, d) => h.OnSubmit(d);
        public static EventFunction<ICancelHandler> cancelHandler { get; } = (h, d) => h.OnCancel(d);

        static bool ShouldSendToComponent<T>(Component c) where T : IEventSystemHandler =>
            c is T && (c is not Behaviour b || b.isActiveAndEnabled);

        static void GetEventList<T>(GameObject go, List<IEventSystemHandler> results) where T : IEventSystemHandler
        {
            results.Clear();
            if (go == null || !go.activeInHierarchy)
                return;
            foreach (Component c in go.GetComponents<Component>())
                if (ShouldSendToComponent<T>(c))
                    results.Add(c as IEventSystemHandler);
        }

        /// <summary>The event to every enabled handler of it on the GameObject; whether there was one.</summary>
        public static bool Execute<T>(GameObject target, BaseEventData eventData, EventFunction<T> functor) where T : IEventSystemHandler
        {
            var handlers = new List<IEventSystemHandler>();
            GetEventList<T>(target, handlers);
            foreach (IEventSystemHandler h in handlers)
            {
                try
                {
                    functor((T)h, eventData);
                }
                catch (Exception e)
                {
                    Debug.LogException(e);
                }
            }
            return handlers.Count > 0;
        }

        /// <summary>To the first GameObject from root up that handles it; that GameObject.</summary>
        public static GameObject ExecuteHierarchy<T>(GameObject root, BaseEventData eventData, EventFunction<T> callbackFunction) where T : IEventSystemHandler
        {
            for (Transform t = root != null ? root.transform : null; t != null; t = t.parent)
                if (Execute(t.gameObject, eventData, callbackFunction))
                    return t.gameObject;
            return null;
        }

        public static bool CanHandleEvent<T>(GameObject go) where T : IEventSystemHandler
        {
            var handlers = new List<IEventSystemHandler>();
            GetEventList<T>(go, handlers);
            return handlers.Count != 0;
        }

        public static GameObject GetEventHandler<T>(GameObject root) where T : IEventSystemHandler
        {
            for (Transform t = root != null ? root.transform : null; t != null; t = t.parent)
                if (CanHandleEvent<T>(t.gameObject))
                    return t.gameObject;
            return null;
        }

        // The native event system's event on a component of a C# class (data null: whether it handles it).
        internal static bool ExecuteOnComponent(Component c, EventKind kind, BaseEventData data)
        {
            if (c is Behaviour b && !b.isActiveAndEnabled)
                return false;
            switch (kind)
            {
                case EventKind.PointerEnter: return Run<IPointerEnterHandler>(c, data, pointerEnterHandler);
                case EventKind.PointerExit: return Run<IPointerExitHandler>(c, data, pointerExitHandler);
                case EventKind.PointerMove: return Run<IPointerMoveHandler>(c, data, pointerMoveHandler);
                case EventKind.PointerDown: return Run<IPointerDownHandler>(c, data, pointerDownHandler);
                case EventKind.PointerUp: return Run<IPointerUpHandler>(c, data, pointerUpHandler);
                case EventKind.PointerClick: return Run<IPointerClickHandler>(c, data, pointerClickHandler);
                case EventKind.InitializePotentialDrag: return Run<IInitializePotentialDragHandler>(c, data, initializePotentialDrag);
                case EventKind.BeginDrag: return Run<IBeginDragHandler>(c, data, beginDragHandler);
                case EventKind.Drag: return Run<IDragHandler>(c, data, dragHandler);
                case EventKind.EndDrag: return Run<IEndDragHandler>(c, data, endDragHandler);
                case EventKind.Drop: return Run<IDropHandler>(c, data, dropHandler);
                case EventKind.Scroll: return Run<IScrollHandler>(c, data, scrollHandler);
                case EventKind.UpdateSelected: return Run<IUpdateSelectedHandler>(c, data, updateSelectedHandler);
                case EventKind.Select: return Run<ISelectHandler>(c, data, selectHandler);
                case EventKind.Deselect: return Run<IDeselectHandler>(c, data, deselectHandler);
                case EventKind.Move: return Run<IMoveHandler>(c, data, moveHandler);
                case EventKind.Submit: return Run<ISubmitHandler>(c, data, submitHandler);
                case EventKind.Cancel: return Run<ICancelHandler>(c, data, cancelHandler);
                default: return false;
            }
        }

        static bool Run<T>(Component c, BaseEventData data, EventFunction<T> f) where T : IEventSystemHandler
        {
            if (c is not T handler)
                return false;
            if (data != null)
            {
                try
                {
                    f(handler, data);
                }
                catch (Exception e)
                {
                    Debug.LogException(e);
                }
            }
            return true;
        }
    }

    public partial class PointerEventData
    {
        readonly List<GameObject> m_Hovered = new List<GameObject>();

        /// <summary>The objects under the pointer it entered (a copy as of reading).</summary>
        public List<GameObject> hovered
        {
            get
            {
                m_Hovered.Clear();
                m_Hovered.AddRange(HoveredInternal());
                return m_Hovered;
            }
        }

        public RaycastResult pointerCurrentRaycast
        {
            get => RaycastResult.From(pointerCurrentRaycastInternal);
            set => pointerCurrentRaycastInternal = value.ToData();
        }

        public RaycastResult pointerPressRaycast
        {
            get => RaycastResult.From(pointerPressRaycastInternal);
            set => pointerPressRaycastInternal = value.ToData();
        }

        public override string ToString() => Describe();
    }

    public abstract partial class BaseRaycaster
    {
        /// <summary>The hits of the pointer appended to the list.</summary>
        public abstract void Raycast(PointerEventData eventData, List<RaycastResult> resultAppendList);

        public override string ToString() => Describe();
    }

    public partial class PhysicsRaycaster
    {
        protected const int kNoEventMaskSet = -1;
        protected Camera m_EventCamera;
        protected int m_LastMaxRayIntersections;

        protected LayerMask m_EventMask
        {
            get => new LayerMask { value = eventMaskValue };
            set => eventMaskValue = value.value;
        }

        protected int m_MaxRayIntersections
        {
            get => maxRayIntersections;
            set => maxRayIntersections = value;
        }

        public LayerMask eventMask
        {
            get => new LayerMask { value = eventMaskValue };
            set => eventMaskValue = value.value;
        }

        public override void Raycast(PointerEventData eventData, List<RaycastResult> resultAppendList)
        {
            foreach (RaycastResultData d in RaycastNative(eventData))
                resultAppendList.Add(RaycastResult.From(d));
        }
    }

    public partial class PanelRaycaster
    {
        /// <summary>The panel it raycasts (its views' panel).</summary>
        public HelixEngine.UI.IPanel panel
        {
            get => panelViewInternal?.rootElement?.panel;
            set => panelId = PanelIdOf(value);
        }

        // The id of the panel whose first view's panel it is (0: none).
        internal static ulong PanelIdOf(HelixEngine.UI.IPanel panel)
        {
            if (panel == null)
                return 0;
            foreach (ulong id in PanelIdsInternal())
                if (PanelViewOfInternal(id)?.rootElement?.panel == panel)
                    return id;
            return 0;
        }

        public override void Raycast(PointerEventData eventData, List<RaycastResult> resultAppendList)
        {
            foreach (RaycastResultData d in RaycastNative(eventData))
                resultAppendList.Add(RaycastResult.From(d));
        }
    }

    public partial class PanelEventHandler
    {
        public HelixEngine.UI.IPanel panel
        {
            get => panelViewInternal?.rootElement?.panel;
            set => panelId = PanelRaycaster.PanelIdOf(value);
        }
    }

    public partial class Physics2DRaycaster
    {
        public override void Raycast(PointerEventData eventData, List<RaycastResult> resultAppendList)
        {
            foreach (RaycastResultData d in RaycastNative(eventData))
                resultAppendList.Add(RaycastResult.From(d));
        }
    }

    public static partial class RaycasterManager
    {
        public static List<BaseRaycaster> GetRaycasters() => new List<BaseRaycaster>(GetRaycastersInternal());
    }

    public partial class EventSystem
    {
        public void RaycastAll(PointerEventData eventData, List<RaycastResult> raycastResults)
        {
            raycastResults.Clear();
            foreach (RaycastResultData d in RaycastAllInternal(eventData))
                raycastResults.Add(RaycastResult.From(d));
        }

        [Obsolete("Use the input module's settings: the canvas UI and the panels share one event system", false)]
        public static void SetUITookitEventSystemOverride(EventSystem activeEventSystem, bool sendEvents = true, bool createPanelGameObjectsOnStart = true)
        {
            if (activeEventSystem != null)
                current = activeEventSystem;
        }

        public override string ToString() => Describe();
    }

    public abstract partial class InputModule
    {
        protected List<RaycastResult> m_RaycastResultCache = new List<RaycastResult>();

        protected BaseInput m_InputOverride
        {
            get => inputOverride;
            set => inputOverride = value;
        }

        protected static RaycastResult FindFirstRaycast(List<RaycastResult> candidates)
        {
            foreach (RaycastResult r in candidates)
                if (r.gameObject != null)
                    return r;
            return new RaycastResult();
        }
    }

    public abstract partial class PointerInputModule
    {
        public const int kMouseLeftId = -1;
        public const int kMouseRightId = -2;
        public const int kMouseMiddleId = -3;
        public const int kFakeTouchesId = -4;

        readonly MouseState m_MouseState = new MouseState();

        protected class ButtonState
        {
            public PointerEventData.InputButton button { get; set; }
            public MouseButtonEventData eventData { get; set; }
        }

        protected class MouseState
        {
            readonly List<ButtonState> m_TrackedButtons = new List<ButtonState>();

            public bool AnyPressesThisFrame()
            {
                foreach (ButtonState b in m_TrackedButtons)
                    if (b.eventData.PressedThisFrame())
                        return true;
                return false;
            }

            public bool AnyReleasesThisFrame()
            {
                foreach (ButtonState b in m_TrackedButtons)
                    if (b.eventData.ReleasedThisFrame())
                        return true;
                return false;
            }

            public ButtonState GetButtonState(PointerEventData.InputButton button)
            {
                foreach (ButtonState b in m_TrackedButtons)
                    if (b.button == button)
                        return b;
                var created = new ButtonState { button = button, eventData = new MouseButtonEventData() };
                m_TrackedButtons.Add(created);
                return created;
            }

            public void SetButtonState(PointerEventData.InputButton button, PointerEventData.FramePressState stateForMouseButton, PointerEventData data)
            {
                ButtonState toModify = GetButtonState(button);
                toModify.eventData.buttonState = stateForMouseButton;
                toModify.eventData.buttonData = data;
            }
        }

        public class MouseButtonEventData
        {
            public PointerEventData.FramePressState buttonState;
            public PointerEventData buttonData;

            public bool PressedThisFrame() => buttonState == PointerEventData.FramePressState.Pressed || buttonState == PointerEventData.FramePressState.PressedAndReleased;

            public bool ReleasedThisFrame() => buttonState == PointerEventData.FramePressState.Released || buttonState == PointerEventData.FramePressState.PressedAndReleased;
        }

        protected virtual MouseState GetMousePointerEventData() => GetMousePointerEventData(0);

        /// <summary>The mouse's three buttons this frame: the left one's data moved and raycast, the others copied from it.</summary>
        protected virtual MouseState GetMousePointerEventData(int id)
        {
            for (int b = 0; b < 3; b++)
            {
                MouseButtonInternal(id, b, out PointerEventData data, out int state);
                m_MouseState.SetButtonState((PointerEventData.InputButton)b, (PointerEventData.FramePressState)state, data);
            }
            return m_MouseState;
        }

        public override string ToString() => Describe();
    }

    public partial class StandaloneInputModule
    {
        protected void ProcessMousePress(MouseButtonEventData data) => ProcessMousePressInternal(data.buttonData, (int)data.buttonState);
    }

    public partial class EventTrigger
    {
        [Serializable]
        public class TriggerEvent : HelixEvent<BaseEventData>
        {
        }

        [Serializable]
        public class Entry
        {
            internal EventTrigger m_Owner;
            internal int m_Index = -1;
            EventTriggerType m_EventID = EventTriggerType.PointerClick;

            public EventTriggerType eventID
            {
                get => m_Owner != null ? m_Owner.EntryType(m_Index) : m_EventID;
                set
                {
                    m_EventID = value;
                    if (m_Owner != null)
                        m_Owner.SetEntryType(m_Index, value);
                }
            }

            public TriggerEvent callback = new TriggerEvent();
        }

        readonly List<Entry> m_Entries = new List<Entry>();
        bool m_Hooked;

        // The entries as the native trigger has them (their callbacks its native events by index).
        void Sync()
        {
            if (!m_Hooked)
            {
                m_Hooked = true;
                entryRaised += (i, data) =>
                {
                    if (i >= 0 && i < m_Entries.Count)
                        NativeEvents.Tell(m_Entries[i].callback, data);
                };
            }
            int n = EntryCount();
            while (m_Entries.Count > n)
                m_Entries.RemoveAt(m_Entries.Count - 1);
            while (m_Entries.Count < n)
                m_Entries.Add(new Entry());
            for (int i = 0; i < n; i++)
            {
                Entry e = m_Entries[i];
                e.m_Owner = this;
                e.m_Index = i;
                int index = i;
                NativeEvents.Bind(e.callback, this, i, args => RaiseEntry(index, args.Length > 0 ? args[0] as BaseEventData : null));
            }
        }

        /// <summary>The trigger's entries (changes to the list itself are applied when it is set).</summary>
        public List<Entry> triggers
        {
            get
            {
                Sync();
                return m_Entries;
            }
            set
            {
                ClearEntries();
                m_Entries.Clear();
                if (value != null)
                    foreach (Entry e in value)
                    {
                        AddEntry(e.eventID);
                        m_Entries.Add(e);
                    }
                Sync();
            }
        }

        [Obsolete("Please use triggers instead (UnityUpgradable) -> triggers", true)]
        public List<Entry> delegates
        {
            get => triggers;
            set => triggers = value;
        }
    }
}

namespace HelixEngine.CanvasUI
{
    using HelixEngine.UIEvents;

    public partial class GraphicRaycaster
    {
        protected const int kNoEventMaskSet = -1;

        public LayerMask blockingMask
        {
            get => new LayerMask { value = blockingMaskValue };
            set => blockingMaskValue = value.value;
        }

        public override void Raycast(PointerEventData eventData, List<RaycastResult> resultAppendList)
        {
            foreach (RaycastResultData d in RaycastNative(eventData))
                resultAppendList.Add(RaycastResult.From(d));
        }
    }
}
