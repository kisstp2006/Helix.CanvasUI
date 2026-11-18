using System.Collections.Generic;
using System.Text;
using HelixEngine;
using HelixEngine.CanvasUI;
using Helix.Scripting;
using HelixEngine.UIEvents;
using HelixEngine.Text;

namespace Tests
{
    // What the C# tests write for the C++ side to read.
    static unsafe class Notes
    {
        static delegate* unmanaged[Cdecl]<byte*, int, void> s_Note;

        public static void Note(GameBehaviour b, string message)
        {
            if (s_Note == null)
                s_Note = (delegate* unmanaged[Cdecl]<byte*, int, void>)Host.FindExport("CanvasUITests.Note");
            byte[] bytes = Encoding.UTF8.GetBytes(b.gameObject.name + ":" + message);
            fixed (byte* p = bytes)
                s_Note(p, bytes.Length);
        }
    }

    static class Make
    {
        public static Canvas Canvas(string name)
        {
            var c = new GameObject(name).AddComponent<Canvas>();
            c.renderMode = RenderMode.ScreenSpaceOverlay;
            c.gameObject.AddComponent<GraphicRaycaster>();
            return c;
        }

        // A RectTransform of the size at the anchored position (anchors and pivot centered) under the parent.
        public static RectTransform Rect(string name, Transform parent, Vector2 position, Vector2 size)
        {
            var r = new GameObject(name).AddComponent<RectTransform>();
            r.SetParent(parent, false);
            r.sizeDelta = size;
            r.anchoredPosition = position;
            return r;
        }
    }

    // A graphic of C#: its own mesh (two quads) from OnPopulateMesh.
    public class CsTwoQuads : MaskableGraphic
    {

        protected override void OnPopulateMesh(VertexHelper vh)
        {
            vh.Clear();
            Rect r = GetPixelAdjustedRect();
            for (int q = 0; q < 2; q++)
            {
                float x0 = r.xMin + q * r.width * 0.5f, x1 = x0 + r.width * 0.5f;
                int start = vh.currentVertCount;
                vh.AddVert(new Vector3(x0, r.yMin), color, new Vector4(0, 0));
                vh.AddVert(new Vector3(x0, r.yMax), color, new Vector4(0, 1));
                vh.AddVert(new Vector3(x1, r.yMax), color, new Vector4(1, 1));
                vh.AddVert(new Vector3(x1, r.yMin), color, new Vector4(1, 0));
                vh.AddTriangle(start, start + 1, start + 2);
                vh.AddTriangle(start + 2, start + 3, start);
            }
        }
    }

    // A mesh effect of C#: every vertex moved up by 5.
    public class CsRaise : BaseMeshEffect
    {
        // What it last saw: the vertices and the first one raised, the triangles' indices.
        public int count, indices;
        public Vector3 first, sixth;

        public override void ModifyMesh(VertexHelper vh)
        {
            if (!IsActive())
                return;
            var v = new UIVertex();
            for (int i = 0; i < vh.currentVertCount; i++)
            {
                vh.PopulateUIVertex(ref v, i);
                v.position.y += 5;
                vh.SetUIVertex(v, i);
            }
            count = vh.currentVertCount;
            indices = vh.currentIndexCount;
            vh.PopulateUIVertex(ref v, 0);
            first = v.position;
            vh.PopulateUIVertex(ref v, 5);
            sixth = v.position;
        }
    }

    // A layout element of C#: preferred 30 x 10, flexible width 1.
    public class CsLayoutSize : GameBehaviour, ILayoutElement
    {
        public void CalculateLayoutInputHorizontal() { }
        public void CalculateLayoutInputVertical() { }
        public float minWidth => 10;
        public float preferredWidth => 30;
        public float flexibleWidth => 1;
        public float minHeight => 10;
        public float preferredHeight => 10;
        public float flexibleHeight => 0;
        public int layoutPriority => 1;
    }

    // Graphics, mesh effects and layout from C#.
    public class CsCanvasGraphics : GameBehaviour
    {
        void Start()
        {
            Canvas canvas = Make.Canvas("Canvas");
            // An image: its quad from the rebuild.
            RectTransform ir = Make.Rect("Image", canvas.transform, new Vector2(0, 0), new Vector2(40, 20));
            var image = ir.gameObject.AddComponent<Image>();
            image.color = new Color(1, 0, 0, 1);
            Canvas.ForceUpdateCanvases();
            Notes.Note(this, $"image:{image.canvasRenderer.GetMaterial() == Canvas.GetDefaultCanvasMaterial()}:{image.depth}:{image.mainTexture == Texture2D.whiteTexture}:{image.canvas == canvas}");
            // A C# graphic's own mesh, raised by a C# effect.
            RectTransform qr = Make.Rect("Quads", canvas.transform, new Vector2(0, 30), new Vector2(40, 20));
            var quads = qr.gameObject.AddComponent<CsTwoQuads>();
            var raise = qr.gameObject.AddComponent<CsRaise>();
            Canvas.ForceUpdateCanvases();
            Notes.Note(this, $"quads:{raise.count}:{raise.indices}:{raise.first.x}:{raise.first.y}:{raise.sixth.x}:{quads.canvasRenderer.cull}");
            // The cull event: a graphic outside its RectMask2D is culled and tells it.
            RectTransform mr = Make.Rect("Mask", canvas.transform, new Vector2(-80, 0), new Vector2(20, 20));
            mr.gameObject.AddComponent<RectMask2D>();
            RectTransform cr = Make.Rect("Culled", mr, new Vector2(100, 0), new Vector2(10, 10));
            var culled = cr.gameObject.AddComponent<Image>();
            var culls = new List<bool>();
            culled.onCullStateChanged.AddListener(v => culls.Add(v));
            Canvas.ForceUpdateCanvases();
            Notes.Note(this, $"cull:{culled.canvasRenderer.cull}:{string.Join(",", culls)}");
            // A horizontal group with a C# layout element and a native one.
            RectTransform gr = Make.Rect("Group", canvas.transform, new Vector2(0, -30), new Vector2(100, 20));
            var group = gr.gameObject.AddComponent<HorizontalLayoutGroup>();
            group.childForceExpandWidth = false;
            group.childControlWidth = true;
            RectTransform a = Make.Rect("A", gr, Vector2.zero, Vector2.zero);
            a.gameObject.AddComponent<CsLayoutSize>();
            RectTransform b = Make.Rect("B", gr, Vector2.zero, Vector2.zero);
            var le = b.gameObject.AddComponent<LayoutElement>();
            le.preferredWidth = 20;
            LayoutRebuilder.ForceRebuildLayoutImmediate(gr);
            Notes.Note(this, $"layout:{a.rect.width}:{b.rect.width}:{LayoutUtility.GetPreferredWidth(gr)}");
            // The vertex helper from C# (disposed with using).
            using (var vh = new VertexHelper())
            {
                vh.AddUIVertexQuad(new[] { UIVertex.simpleVert, UIVertex.simpleVert, UIVertex.simpleVert, UIVertex.simpleVert });
                Notes.Note(this, $"vh:{vh.currentVertCount}:{vh.currentIndexCount}");
            }
            Object.DestroyImmediate(canvas.gameObject);
        }
    }

    // A C# handler of clicks and enters.
    public class CsClicks : GameBehaviour, IPointerClickHandler, IPointerEnterHandler
    {
        public int clicks, enters;

        public void OnPointerClick(PointerEventData eventData) => clicks++;
        public void OnPointerEnter(PointerEventData eventData) => enters++;
    }

    // A raycaster of C#: the screen's left 20 pixels are its GameObject's.
    public class CsStrip : BaseRaycaster
    {
        public override Camera eventCamera => null;
        public override int sortOrderPriority => 100;

        public override void Raycast(PointerEventData eventData, List<RaycastResult> resultAppendList)
        {
            if (eventData.position.x >= 20)
                return;
            resultAppendList.Add(new RaycastResult { gameObject = gameObject, module = this, screenPosition = eventData.position, index = resultAppendList.Count });
        }
    }

    // The event system, the controls and their events from C# (the C++ side clicks; Check writes what was heard).
    public class CsCanvasEvents : GameBehaviour
    {
        int m_Clicks, m_Triggers;
        readonly List<bool> m_Toggles = new List<bool>();
        readonly List<float> m_Sliders = new List<float>();
        CsClicks m_Handler, m_StripHandler;
        Toggle m_Toggle;
        Slider m_Slider;
        Canvas m_Canvas;

        void Start()
        {
            var es = new GameObject("EventSystem").AddComponent<EventSystem>();
            es.gameObject.AddComponent<StandaloneInputModule>();
            m_Canvas = Make.Canvas("Canvas");
            // A button at (100, 50) (40 x 20), heard in C#.
            RectTransform br = Make.Rect("Button", m_Canvas.transform, new Vector2(0, 0), new Vector2(40, 20));
            var button = br.gameObject.AddComponent<Button>();
            button.targetGraphic = br.gameObject.AddComponent<Image>();
            button.onClick.AddListener(() => m_Clicks++);
            // An image at (150, 50) with a C# handler.
            RectTransform hr = Make.Rect("Handler", m_Canvas.transform, new Vector2(50, 0), new Vector2(20, 20));
            hr.gameObject.AddComponent<Image>();
            m_Handler = hr.gameObject.AddComponent<CsClicks>();
            // A toggle at (50, 50); a slider at (100, 20) (0 to 10 over x 80 to 120); a trigger at (150, 20).
            RectTransform tr = Make.Rect("Toggle", m_Canvas.transform, new Vector2(-50, 0), new Vector2(20, 20));
            m_Toggle = tr.gameObject.AddComponent<Toggle>();
            m_Toggle.targetGraphic = tr.gameObject.AddComponent<Image>();
            m_Toggle.onValueChanged.AddListener(v => m_Toggles.Add(v));
            RectTransform sr = Make.Rect("Slider", m_Canvas.transform, new Vector2(0, -30), new Vector2(40, 10));
            m_Slider = sr.gameObject.AddComponent<Slider>();
            m_Slider.targetGraphic = sr.gameObject.AddComponent<Image>();
            RectTransform area = Make.Rect("Fill Area", sr, Vector2.zero, Vector2.zero);
            area.anchorMin = Vector2.zero;
            area.anchorMax = Vector2.one;
            m_Slider.fillRect = Make.Rect("Fill", area, Vector2.zero, Vector2.zero);
            m_Slider.maxValue = 10;
            m_Slider.onValueChanged.AddListener(v => m_Sliders.Add(v));
            RectTransform er = Make.Rect("Trigger", m_Canvas.transform, new Vector2(50, -30), new Vector2(20, 10));
            er.gameObject.AddComponent<Image>();
            var trigger = er.gameObject.AddComponent<EventTrigger>();
            var entry = new EventTrigger.Entry { eventID = EventTriggerType.PointerClick };
            entry.callback.AddListener(d => m_Triggers++);
            trigger.triggers = new List<EventTrigger.Entry> { entry };
            // A raycaster of C# with a C# handler on its GameObject.
            var strip = new GameObject("Strip").AddComponent<CsStrip>();
            m_StripHandler = strip.gameObject.AddComponent<CsClicks>();
            Canvas.ForceUpdateCanvases();
            // Raycasts from C#: the button under its center, the strip at the left.
            var data = new PointerEventData(es) { position = new Vector2(100, 50) };
            var hits = new List<RaycastResult>();
            es.RaycastAll(data, hits);
            string first = hits.Count > 0 ? hits[0].gameObject.name : "";
            data.position = new Vector2(10, 50);
            es.RaycastAll(data, hits);
            Notes.Note(this, $"raycast:{first}:{hits.Count}:{(hits.Count > 0 ? hits[0].gameObject.name : "")}:{hits.Count > 0 && hits[0].module is CsStrip}");
            // ExecuteEvents from C# on the C# handler and on the native button.
            bool handled = ExecuteEvents.Execute(m_Handler.gameObject, data, ExecuteEvents.pointerClickHandler);
            bool native = ExecuteEvents.Execute(button.gameObject, data, ExecuteEvents.pointerClickHandler);
            Notes.Note(this, $"execute:{handled}:{m_Handler.clicks}:{native}:{m_Clicks}:{ExecuteEvents.CanHandleEvent<ISubmitHandler>(button.gameObject)}:{ExecuteEvents.GetEventHandler<IPointerClickHandler>(br.gameObject) == br.gameObject}");
            m_Handler.clicks = 0;
            m_Clicks = 0;
            // The structs over the native ones.
            Navigation nav = button.navigation;
            nav.mode = Navigation.Mode.Explicit;
            nav.selectOnRight = m_Toggle;
            button.navigation = nav;
            ColorBlock colors = button.colors;
            colors.pressedColor = new Color(0, 1, 0, 1);
            button.colors = colors;
            Notes.Note(this, $"structs:{button.navigation.mode}:{button.navigation.selectOnRight == m_Toggle}:{button.colors.pressedColor.g}:{ColorBlock.defaultColorBlock.fadeDuration}:{Selectable.allSelectableCount}");
        }

        void Check()
        {
            Notes.Note(this, $"heard:{m_Clicks}:{m_Handler.clicks}:{m_Handler.enters}:{string.Join(",", m_Toggles)}:{m_Sliders.Count > 0 && Mathf.Abs(m_Sliders[m_Sliders.Count - 1] - 7.5f) < 0.01f}:{m_Triggers}:{m_StripHandler.clicks}");
            Object.DestroyImmediate(m_Canvas.gameObject);
        }
    }

    // The default controls from C#, through a factory of C#.
    public class CsDefaultControls : GameBehaviour
    {
        sealed class Counting : DefaultControls.IFactoryControls
        {
            public readonly List<string> names = new List<string>();

            public GameObject CreateGameObject(string name, params System.Type[] components)
            {
                names.Add(name);
                var go = new GameObject(name);
                foreach (System.Type t in components)
                    go.AddComponent(t);
                return go;
            }
        }

        void Start()
        {
            var resources = new DefaultControls.Resources();
            GameObject slider = DefaultControls.CreateSlider(resources);
            var counting = new Counting();
            DefaultControls.factory = counting;
            GameObject view = DefaultControls.CreateScrollView(resources);
            DefaultControls.factory = null;
            var scroll = view.GetComponent<ScrollRect>();
            Notes.Note(this, $"defaults:{slider.name}:{slider.GetComponent<Slider>().fillRect.name}:{counting.names.Count}:{counting.names[0]}:{scroll.verticalScrollbar.name}:{scroll.verticalScrollbarVisibility}");
            Object.DestroyImmediate(slider);
            Object.DestroyImmediate(view);
        }
    }
    // Digits only: the validator puts the character in itself.
    public class CsDigitsOnly : InputValidator
    {
        public override char Validate(ref string text, ref int pos, char ch)
        {
            if (ch < '0' || ch > '9')
                return '\0';
            text = text.Insert(pos, ch.ToString());
            pos++;
            return ch;
        }
    }

    public class CsTextControls : GameBehaviour
    {
        TextInput m_Upper, m_Digits;
        Dropdown m_Dropdown;
        string m_Changed = "";
        int m_Picked = -1;

        void Start()
        {
            Canvas canvas = Make.Canvas("Text Canvas");
            var resources = new TextDefaultControls.Resources();
            m_Upper = TextDefaultControls.CreateInputField(resources).GetComponent<TextInput>();
            m_Upper.transform.SetParent(canvas.transform, false);
            m_Upper.onValidateInput = (text, index, added) => char.ToUpperInvariant(added);
            m_Upper.onValueChanged.AddListener(v => m_Changed = v);
            m_Upper.onFocusSelectAll = false;
            m_Upper.ActivateInputField();

            m_Digits = TextDefaultControls.CreateInputField(resources).GetComponent<TextInput>();
            m_Digits.transform.SetParent(canvas.transform, false);
            m_Digits.inputValidator = ScriptableObject.CreateInstance<CsDigitsOnly>();
            m_Digits.onFocusSelectAll = false;

            m_Dropdown = TextDefaultControls.CreateDropdown(resources).GetComponent<Dropdown>();
            m_Dropdown.transform.SetParent(canvas.transform, false);
            m_Dropdown.ClearOptions();
            m_Dropdown.AddOptions(new List<string> { "Red", "Green" });
            m_Dropdown.options.Add(new Dropdown.OptionData("Blue"));
            m_Dropdown.onValueChanged.AddListener(v => m_Picked = v);
            m_Dropdown.value = 2;
        }

        void FocusDigits() => m_Digits.ActivateInputField();

        void Check()
        {
            Notes.Note(this, $"text:{m_Upper.text}:{m_Changed}:{m_Digits.text}:{m_Digits.characterValidation}");
            Notes.Note(this, $"dropdown:{m_Dropdown.options.Count}:{m_Dropdown.value}:{m_Picked}:{m_Dropdown.captionText.text}:{m_Dropdown.template.name}");
        }
    }
}
