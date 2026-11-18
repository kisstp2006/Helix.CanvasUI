using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using HelixEngine.Events;

namespace HelixEngine.CanvasUI
{
    // The C# side's part of the native canvas UI: its components of C# classes answer for the canvas UI's interfaces
    // they implement (the native code asks through the internal events of ManagedCalls).
    internal static class CanvasUIHooks
    {
#pragma warning disable CA2255
        [ModuleInitializer]
#pragma warning restore CA2255
        internal static void Init() => HelixEngine.Internal.ScriptBridge.WhenBindingsLoaded(Start);

        // The hooks once the module's bindings reach the native side (the initializer runs as they load).
        static void Start()
        {
            HelixEngine.Text.TextBase.textChanged += t => HelixEngine.Text.TextEventManager.ON_TEXT_CHANGED(t);
            ManagedCalls.modifyMaterial += (c, m) =>
            {
                if (c is IMaterialModifier modifier)
                    ManagedCalls.SetMaterial(modifier.GetModifiedMaterial(m));
            };
            ManagedCalls.modifyMesh += (c, vh) =>
            {
                if (c is IMeshModifier modifier)
                    modifier.ModifyMesh(vh);
            };
            ManagedCalls.raycastFilter += (c, sp, camera) =>
            {
                if (c is ICanvasRaycastFilter filter)
                    ManagedCalls.SetBool(filter.IsRaycastLocationValid(sp, camera));
            };
            ManagedCalls.implements += (c, name) => ManagedCalls.SetBool(Implements(c, name));
            ManagedCalls.layout += Layout;
            CanvasUpdateRegistry.managedRebuild += CanvasUpdateRegistry.Managed;
            Graphic.dirtyCallback += Graphic.RaiseDirty;
        }

        static bool Implements(Component c, string name) => name switch
        {
            "ICanvasRaycastFilter" => c is ICanvasRaycastFilter,
            "ILayoutElement" => c is ILayoutElement,
            "ILayoutController" => c is ILayoutController,
            "ILayoutSelfController" => c is ILayoutSelfController,
            "ILayoutGroup" => c is ILayoutGroup,
            "ILayoutIgnorer" => c is ILayoutIgnorer,
            "IMaterialModifier" => c is IMaterialModifier,
            "IMeshModifier" => c is IMeshModifier,
            _ => false,
        };

        static void Layout(Component c, int call)
        {
            switch (call)
            {
                case 0: (c as ILayoutElement)?.CalculateLayoutInputHorizontal(); break;
                case 1: (c as ILayoutElement)?.CalculateLayoutInputVertical(); break;
                case 2: (c as ILayoutController)?.SetLayoutHorizontal(); break;
                case 3: (c as ILayoutController)?.SetLayoutVertical(); break;
                case 17: ManagedCalls.SetBool(c is ILayoutIgnorer i && i.ignoreLayout); break;
                default:
                    if (c is ILayoutElement e)
                        ManagedCalls.SetFloat(call switch
                        {
                            10 => e.minWidth,
                            11 => e.preferredWidth,
                            12 => e.flexibleWidth,
                            13 => e.minHeight,
                            14 => e.preferredHeight,
                            15 => e.flexibleHeight,
                            16 => e.layoutPriority,
                            _ => 0,
                        });
                    break;
            }
        }

        // The object's class implements the interface in C++ (its nearest generated class has it).
        internal static bool NativeImplements(object o, Type iface)
        {
            for (Type t = o?.GetType(); t != null; t = t.BaseType)
                if (Helix.Interop.Types.IsGenerated(t))
                    return iface.IsAssignableFrom(t);
            return false;
        }
    }

    public static partial class CanvasUpdateRegistry
    {
        // The elements of C# classes by their keys, and what they are queued for (1 layout, 2 graphic).
        static readonly Dictionary<ICanvasElement, ulong> s_Keys = new(ReferenceEqualityComparer.Instance);
        static readonly Dictionary<ulong, (ICanvasElement element, int queues)> s_Elements = new();
        static ulong s_Next;

        static ulong KeyOf(ICanvasElement e)
        {
            if (!s_Keys.TryGetValue(e, out ulong key))
            {
                key = ++s_Next;
                s_Keys[e] = key;
                s_Elements[key] = (e, 0);
            }
            return key;
        }

        static void Forget(ulong key, int queue)
        {
            if (!s_Elements.TryGetValue(key, out var entry))
                return;
            entry.queues &= ~queue;
            if (entry.queues == 0)
            {
                s_Elements.Remove(key);
                s_Keys.Remove(entry.element);
            }
            else
                s_Elements[key] = entry;
        }

        static bool Add(ICanvasElement element, int queue)
        {
            if (element == null)
                return false;
            if (CanvasUIHooks.NativeImplements(element, typeof(ICanvasElement)))
                return queue == 0 ? TryRegisterCanvasElementForLayoutRebuildNative(element) : TryRegisterCanvasElementForGraphicRebuildNative(element);
            ulong key = KeyOf(element);
            Transform t = null;
            try
            {
                t = element.transform;
            }
            catch (Exception)
            {
            }
            int added = RegisterManaged(key, t, queue);
            if (added == 1)
            {
                var entry = s_Elements[key];
                entry.queues |= queue == 0 ? 1 : 2;
                s_Elements[key] = entry;
            }
            else if (added == 0)
                Forget(key, 0);
            return added == 1;
        }

        public static void RegisterCanvasElementForLayoutRebuild(ICanvasElement element) => Add(element, 0);

        public static bool TryRegisterCanvasElementForLayoutRebuild(ICanvasElement element) => Add(element, 0);

        public static void RegisterCanvasElementForGraphicRebuild(ICanvasElement element) => Add(element, 1);

        public static bool TryRegisterCanvasElementForGraphicRebuild(ICanvasElement element) => Add(element, 1);

        public static void UnRegisterCanvasElementForRebuild(ICanvasElement element) => Remove(element, false);

        public static void DisableCanvasElementForRebuild(ICanvasElement element) => Remove(element, true);

        static void Remove(ICanvasElement element, bool disable)
        {
            if (element == null)
                return;
            if (CanvasUIHooks.NativeImplements(element, typeof(ICanvasElement)))
            {
                if (disable)
                    DisableCanvasElementForRebuildNative(element);
                else
                    UnRegisterCanvasElementForRebuildNative(element);
                return;
            }
            if (s_Keys.TryGetValue(element, out ulong key) && UnregisterManaged(key, disable))
                Forget(key, 3);
        }

        // The native loop's calls on the elements of C# classes.
        internal static void Managed(ulong key, int what)
        {
            if (!s_Elements.TryGetValue(key, out var entry))
            {
                if (what == 12)
                    SetManagedDestroyed(true);
                return;
            }
            ICanvasElement e = entry.element;
            switch (what)
            {
                case 10:
                    e.LayoutComplete();
                    Forget(key, 1);
                    break;
                case 11:
                    e.GraphicUpdateComplete();
                    Forget(key, 2);
                    break;
                case 12:
                    SetManagedDestroyed(e.IsDestroyed());
                    break;
                default:
                    e.Rebuild((CanvasUpdate)what);
                    break;
            }
        }
    }

    public static partial class GraphicRegistry
    {
        public static IList<Graphic> GetGraphicsForCanvas(Canvas canvas) => GetGraphicsForCanvasInternal(canvas);

        public static IList<Graphic> GetRaycastableGraphicsForCanvas(Canvas canvas) => GetRaycastableGraphicsForCanvasInternal(canvas);
    }

    public partial class Graphic
    {
        protected static Material s_DefaultUI = null;
        protected static Texture2D s_WhiteTexture = null;
        protected static Mesh s_Mesh => workerMesh;

        // The dirty callbacks of each graphic (layout, vertices, material).
        static readonly ConditionalWeakTable<Graphic, HelixAction[]> s_Callbacks = new();

        void Register(int kind, HelixAction action, bool add)
        {
            HelixAction[] slots = s_Callbacks.GetOrCreateValue(this);
            if (add)
                slots[kind] += action;
            else
                slots[kind] -= action;
            RegisterDirtyCallbackInternal(kind, slots[kind] != null, 0);
        }

        internal static void RaiseDirty(Graphic g, int kind)
        {
            if (g != null && s_Callbacks.TryGetValue(g, out HelixAction[] slots))
                slots[kind]?.Invoke();
        }

        public void RegisterDirtyLayoutCallback(HelixAction action) => Register(0, action, true);

        public void UnregisterDirtyLayoutCallback(HelixAction action) => Register(0, action, false);

        public void RegisterDirtyVerticesCallback(HelixAction action) => Register(1, action, true);

        public void UnregisterDirtyVerticesCallback(HelixAction action) => Register(1, action, false);

        public void RegisterDirtyMaterialCallback(HelixAction action) => Register(2, action, true);

        public void UnregisterDirtyMaterialCallback(HelixAction action) => Register(2, action, false);

        [Obsolete("Use OnPopulateMesh(VertexHelper vh) instead.", false)]
        protected virtual void OnPopulateMesh(Mesh m)
        {
            var vh = new VertexHelper();
            OnPopulateMesh(vh);
            vh.FillMesh(m);
        }
    }

    public partial class VertexHelper : IDisposable
    {
        public void AddUIVertexStream(List<UIVertex> verts, List<int> indices) => AddUIVertexStreamInternal(verts.ToArray(), indices.ToArray());

        public void AddUIVertexTriangleStream(List<UIVertex> verts)
        {
            if (verts != null)
                AddUIVertexTriangleStreamInternal(verts.ToArray());
        }

        public void GetUIVertexStream(List<UIVertex> stream)
        {
            if (stream == null)
                return;
            stream.Clear();
            stream.AddRange(GetUIVertexStreamInternal());
        }
    }

    public partial class LayoutRebuilder
    {
        public override string ToString() => Describe();
    }

    public static partial class LayoutUtility
    {
        public static float GetLayoutProperty(RectTransform rect, Func<ILayoutElement, float> property, float defaultValue) =>
            GetLayoutProperty(rect, property, defaultValue, out _);

        /// <summary>The highest priority element's value (the largest among equal priorities); negative values are ignored.</summary>
        public static float GetLayoutProperty(RectTransform rect, Func<ILayoutElement, float> property, float defaultValue, out ILayoutElement source)
        {
            source = null;
            if (rect == null)
                return 0;
            float min = defaultValue;
            int maxPriority = int.MinValue;
            foreach (Component c in rect.GetComponents<Component>())
            {
                if (c is not ILayoutElement e || (c is Behaviour b && !b.isActiveAndEnabled))
                    continue;
                int priority = e.layoutPriority;
                if (priority < maxPriority)
                    continue;
                float value = property(e);
                if (value < 0)
                    continue;
                if (priority > maxPriority)
                {
                    min = value;
                    maxPriority = priority;
                    source = e;
                }
                else if (value > min)
                {
                    min = value;
                    source = e;
                }
            }
            return min;
        }
    }

    public static partial class MaskUtilities
    {
        public static void GetRectMasksForClip(RectMask2D clipper, List<RectMask2D> masks)
        {
            masks.Clear();
            masks.AddRange(GetRectMasksForClipInternal(clipper));
        }
    }

    public static partial class Clipping
    {
        public static Rect FindCullAndClipWorldRect(List<RectMask2D> rectMaskParents, out bool validRect) =>
            FindCullAndClipWorldRect(rectMaskParents.ToArray(), out validRect);
    }

    public partial class Mask : ICanvasRaycastFilter
    {
    }

    public partial class RectMask2D : ICanvasRaycastFilter
    {
    }

    public partial class Image : ICanvasRaycastFilter, ISerializationCallbackReceiver
    {
        protected static Material s_ETC1DefaultUI = null;
    }

    public partial class Shadow
    {
        protected void ApplyShadowZeroAlloc(List<UIVertex> verts, Color32 color, int start, int end, float x, float y)
        {
            int needed = verts.Count + end - start;
            if (verts.Capacity < needed)
                verts.Capacity = needed;
            for (int i = start; i < end; ++i)
            {
                UIVertex vt = verts[i];
                verts.Add(vt);
                Vector3 v = vt.position;
                v.x += x;
                v.y += y;
                vt.position = v;
                Color32 c = color;
                if (useGraphicAlpha)
                    c.a = (byte)(c.a * verts[i].color.a / 255);
                vt.color = c;
                verts[i] = vt;
            }
        }

        protected void ApplyShadow(List<UIVertex> verts, Color32 color, int start, int end, float x, float y) =>
            ApplyShadowZeroAlloc(verts, color, start, end, x, y);
    }

    [Obsolete("Use BaseMeshEffect instead", true)]
    public abstract class BaseVertexEffect
    {
        [Obsolete("Use BaseMeshEffect.ModifyMeshes instead", true)]
        public abstract void ModifyVertices(List<UIVertex> vertices);
    }
}
