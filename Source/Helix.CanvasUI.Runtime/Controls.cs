using System;
using System.Collections.Generic;
using HelixEngine.Events;
using HelixEngine.Internal;
using HelixEngine.UIEvents;

namespace HelixEngine.CanvasUI
{
    [Serializable]
    public struct Navigation : IEquatable<Navigation>
    {
        [Flags]
        public enum Mode
        {
            None = 0,
            Horizontal = 1,
            Vertical = 2,
            Automatic = 3,
            Explicit = 4,
        }

        [SerializeField] Mode m_Mode;
        [SerializeField] bool m_WrapAround;
        [SerializeField] Selectable m_SelectOnUp;
        [SerializeField] Selectable m_SelectOnDown;
        [SerializeField] Selectable m_SelectOnLeft;
        [SerializeField] Selectable m_SelectOnRight;

        public Mode mode { get => m_Mode; set => m_Mode = value; }
        public bool wrapAround { get => m_WrapAround; set => m_WrapAround = value; }
        public Selectable selectOnUp { get => m_SelectOnUp; set => m_SelectOnUp = value; }
        public Selectable selectOnDown { get => m_SelectOnDown; set => m_SelectOnDown = value; }
        public Selectable selectOnLeft { get => m_SelectOnLeft; set => m_SelectOnLeft = value; }
        public Selectable selectOnRight { get => m_SelectOnRight; set => m_SelectOnRight = value; }

        public static Navigation defaultNavigation => new Navigation { m_Mode = Mode.Automatic, m_WrapAround = false };

        public bool Equals(Navigation other) =>
            mode == other.mode && selectOnUp == other.selectOnUp && selectOnDown == other.selectOnDown && selectOnLeft == other.selectOnLeft &&
            selectOnRight == other.selectOnRight;

        internal static Navigation From(NavigationData d) => new Navigation
        {
            m_Mode = (Mode)(int)d.mode,
            m_WrapAround = d.wrapAround,
            m_SelectOnUp = Resources.EntityIdToObject(d.selectOnUp) as Selectable,
            m_SelectOnDown = Resources.EntityIdToObject(d.selectOnDown) as Selectable,
            m_SelectOnLeft = Resources.EntityIdToObject(d.selectOnLeft) as Selectable,
            m_SelectOnRight = Resources.EntityIdToObject(d.selectOnRight) as Selectable,
        };

        internal NavigationData ToData() => new NavigationData
        {
            mode = (NavigationMode)(int)m_Mode,
            wrapAround = m_WrapAround,
            selectOnUp = IdOf(m_SelectOnUp),
            selectOnDown = IdOf(m_SelectOnDown),
            selectOnLeft = IdOf(m_SelectOnLeft),
            selectOnRight = IdOf(m_SelectOnRight),
        };

        internal static EntityId IdOf(Object o) => o != null ? o.GetEntityId() : default;
    }

    [Serializable]
    public struct ColorBlock : IEquatable<ColorBlock>
    {
        [SerializeField] Color m_NormalColor;
        [SerializeField] Color m_HighlightedColor;
        [SerializeField] Color m_PressedColor;
        [SerializeField] Color m_SelectedColor;
        [SerializeField] Color m_DisabledColor;
        [SerializeField] float m_ColorMultiplier;
        [SerializeField] float m_FadeDuration;

        public Color normalColor { get => m_NormalColor; set => m_NormalColor = value; }
        public Color highlightedColor { get => m_HighlightedColor; set => m_HighlightedColor = value; }
        public Color pressedColor { get => m_PressedColor; set => m_PressedColor = value; }
        public Color selectedColor { get => m_SelectedColor; set => m_SelectedColor = value; }
        public Color disabledColor { get => m_DisabledColor; set => m_DisabledColor = value; }
        public float colorMultiplier { get => m_ColorMultiplier; set => m_ColorMultiplier = value; }
        public float fadeDuration { get => m_FadeDuration; set => m_FadeDuration = value; }

        public static ColorBlock defaultColorBlock { get; } = new ColorBlock
        {
            m_NormalColor = new Color32(255, 255, 255, 255),
            m_HighlightedColor = new Color32(245, 245, 245, 255),
            m_PressedColor = new Color32(200, 200, 200, 255),
            m_SelectedColor = new Color32(245, 245, 245, 255),
            m_DisabledColor = new Color32(200, 200, 200, 128),
            m_ColorMultiplier = 1,
            m_FadeDuration = 0.1f,
        };

        public override bool Equals(object obj) => obj is ColorBlock other && Equals(other);

        public bool Equals(ColorBlock other) =>
            normalColor == other.normalColor && highlightedColor == other.highlightedColor && pressedColor == other.pressedColor &&
            selectedColor == other.selectedColor && disabledColor == other.disabledColor && colorMultiplier == other.colorMultiplier &&
            fadeDuration == other.fadeDuration;

        public static bool operator ==(ColorBlock point1, ColorBlock point2) => point1.Equals(point2);
        public static bool operator !=(ColorBlock point1, ColorBlock point2) => !point1.Equals(point2);

        public override int GetHashCode() => base.GetHashCode();

        internal static ColorBlock From(ColorBlockData d) => new ColorBlock
        {
            m_NormalColor = d.normalColor,
            m_HighlightedColor = d.highlightedColor,
            m_PressedColor = d.pressedColor,
            m_SelectedColor = d.selectedColor,
            m_DisabledColor = d.disabledColor,
            m_ColorMultiplier = d.colorMultiplier,
            m_FadeDuration = d.fadeDuration,
        };

        internal ColorBlockData ToData() => new ColorBlockData
        {
            normalColor = m_NormalColor,
            highlightedColor = m_HighlightedColor,
            pressedColor = m_PressedColor,
            selectedColor = m_SelectedColor,
            disabledColor = m_DisabledColor,
            colorMultiplier = m_ColorMultiplier,
            fadeDuration = m_FadeDuration,
        };
    }

    [Serializable]
    public struct SpriteState : IEquatable<SpriteState>
    {
        [SerializeField] Sprite m_HighlightedSprite;
        [SerializeField] Sprite m_PressedSprite;
        [SerializeField] Sprite m_SelectedSprite;
        [SerializeField] Sprite m_DisabledSprite;

        public Sprite highlightedSprite { get => m_HighlightedSprite; set => m_HighlightedSprite = value; }
        public Sprite pressedSprite { get => m_PressedSprite; set => m_PressedSprite = value; }
        public Sprite selectedSprite { get => m_SelectedSprite; set => m_SelectedSprite = value; }
        public Sprite disabledSprite { get => m_DisabledSprite; set => m_DisabledSprite = value; }

        public bool Equals(SpriteState other) =>
            highlightedSprite == other.highlightedSprite && pressedSprite == other.pressedSprite && selectedSprite == other.selectedSprite &&
            disabledSprite == other.disabledSprite;

        internal static SpriteState From(SpriteStateData d) => new SpriteState
        {
            m_HighlightedSprite = Resources.EntityIdToObject(d.highlightedSprite) as Sprite,
            m_PressedSprite = Resources.EntityIdToObject(d.pressedSprite) as Sprite,
            m_SelectedSprite = Resources.EntityIdToObject(d.selectedSprite) as Sprite,
            m_DisabledSprite = Resources.EntityIdToObject(d.disabledSprite) as Sprite,
        };

        internal SpriteStateData ToData() => new SpriteStateData
        {
            highlightedSprite = Navigation.IdOf(m_HighlightedSprite),
            pressedSprite = Navigation.IdOf(m_PressedSprite),
            selectedSprite = Navigation.IdOf(m_SelectedSprite),
            disabledSprite = Navigation.IdOf(m_DisabledSprite),
        };
    }

    public partial class Selectable
    {
        static Selectable[] s_SelectablesStore = new Selectable[10];

        /// <summary>The enabled selectables (filled in when read).</summary>
        protected static Selectable[] s_Selectables
        {
            get
            {
                Selectable[] all = allSelectablesInternal;
                if (s_SelectablesStore.Length < all.Length)
                    s_SelectablesStore = new Selectable[Math.Max(all.Length, s_SelectablesStore.Length * 2)];
                Array.Copy(all, s_SelectablesStore, all.Length);
                return s_SelectablesStore;
            }
            set => s_SelectablesStore = value;
        }

        protected static int s_SelectableCount => allSelectableCount;

        public static Selectable[] allSelectablesArray => allSelectablesInternal;

        [Obsolete("Replaced with allSelectablesArray to have better performance when disabling a element", false)]
        public static List<Selectable> allSelectables => new List<Selectable>(allSelectablesInternal);

        public static int AllSelectablesNoAlloc(Selectable[] selectables)
        {
            Selectable[] all = allSelectablesInternal;
            int copyCount = selectables.Length < all.Length ? selectables.Length : all.Length;
            Array.Copy(all, selectables, copyCount);
            return copyCount;
        }

        public Navigation navigation
        {
            get => Navigation.From(navigationInternal);
            set => navigationInternal = value.ToData();
        }

        public ColorBlock colors
        {
            get => ColorBlock.From(colorsInternal);
            set => colorsInternal = value.ToData();
        }

        public SpriteState spriteState
        {
            get => SpriteState.From(spriteStateInternal);
            set => spriteStateInternal = value.ToData();
        }
    }

    public partial class Button
    {
        [Serializable]
        public class ButtonClickedEvent : HelixEvent
        {
        }

        ButtonClickedEvent m_OnClickEvent;
        bool m_Hooked;

        /// <summary>The click's event: its persistent calls and C++ listeners are the native button's, C#'s are here.</summary>
        public ButtonClickedEvent onClick
        {
            get
            {
                if (m_OnClickEvent == null)
                    m_OnClickEvent = NativeEvents.Bind(new ButtonClickedEvent(), this, 0);
                if (!m_Hooked)
                {
                    m_Hooked = true;
                    onClickNative += () => NativeEvents.Tell(m_OnClickEvent);
                }
                return m_OnClickEvent;
            }
            set
            {
                if (value != null)
                {
                    m_OnClickEvent = NativeEvents.Bind(value, this, 0);
                    _ = onClick;
                }
            }
        }
    }

    public partial class Toggle
    {
        [Serializable]
        public class ToggleEvent : HelixEvent<bool>
        {
        }

        ToggleEvent m_OnValueChangedEvent;
        bool m_Hooked;

        public ToggleEvent onValueChanged
        {
            get
            {
                if (m_OnValueChangedEvent == null)
                    m_OnValueChangedEvent = NativeEvents.Bind(new ToggleEvent(), this, 0);
                if (!m_Hooked)
                {
                    m_Hooked = true;
                    onValueChangedNative += v => NativeEvents.Tell(m_OnValueChangedEvent, v);
                }
                return m_OnValueChangedEvent;
            }
            set
            {
                if (value != null)
                {
                    m_OnValueChangedEvent = NativeEvents.Bind(value, this, 0);
                    _ = onValueChanged;
                }
            }
        }

        /// <summary>The graphic shown while on.</summary>
        public Graphic graphic
        {
            get => graphicInternal;
            set => graphicInternal = value;
        }
    }

    public partial class ToggleGroup
    {
        public IEnumerable<Toggle> ActiveToggles() => ActiveTogglesInternal();

        protected List<Toggle> m_Toggles => new List<Toggle>(TogglesInternal());
    }

    public partial class MaskableGraphic
    {
        [Serializable]
        public class CullStateChangedEvent : HelixEvent<bool>
        {
        }

        CullStateChangedEvent m_OnCullStateChangedEvent;
        bool m_CullHooked;

        public CullStateChangedEvent onCullStateChanged
        {
            get
            {
                if (m_OnCullStateChangedEvent == null)
                    m_OnCullStateChangedEvent = NativeEvents.Bind(new CullStateChangedEvent(), this, 0);
                if (!m_CullHooked)
                {
                    m_CullHooked = true;
                    onCullStateChangedNative += v => NativeEvents.Tell(m_OnCullStateChangedEvent, v);
                }
                return m_OnCullStateChangedEvent;
            }
            set
            {
                if (value != null)
                {
                    m_OnCullStateChangedEvent = NativeEvents.Bind(value, this, 0);
                    _ = onCullStateChanged;
                }
            }
        }
    }

    public partial class Slider
    {
        [Serializable]
        public class SliderEvent : HelixEvent<float>
        {
        }

        SliderEvent m_OnValueChangedEvent;
        bool m_Hooked;

        public SliderEvent onValueChanged
        {
            get
            {
                if (m_OnValueChangedEvent == null)
                    m_OnValueChangedEvent = NativeEvents.Bind(new SliderEvent(), this, 0);
                if (!m_Hooked)
                {
                    m_Hooked = true;
                    onValueChangedNative += v => NativeEvents.Tell(m_OnValueChangedEvent, v);
                }
                return m_OnValueChangedEvent;
            }
            set
            {
                if (value != null)
                {
                    m_OnValueChangedEvent = NativeEvents.Bind(value, this, 0);
                    _ = onValueChanged;
                }
            }
        }
    }

    public partial class Scrollbar
    {
        [Serializable]
        public class ScrollEvent : HelixEvent<float>
        {
        }

        ScrollEvent m_OnValueChangedEvent;
        bool m_Hooked;

        public ScrollEvent onValueChanged
        {
            get
            {
                if (m_OnValueChangedEvent == null)
                    m_OnValueChangedEvent = NativeEvents.Bind(new ScrollEvent(), this, 0);
                if (!m_Hooked)
                {
                    m_Hooked = true;
                    onValueChangedNative += v => NativeEvents.Tell(m_OnValueChangedEvent, v);
                }
                return m_OnValueChangedEvent;
            }
            set
            {
                if (value != null)
                {
                    m_OnValueChangedEvent = NativeEvents.Bind(value, this, 0);
                    _ = onValueChanged;
                }
            }
        }
    }

    public partial class ScrollRect
    {
        [Serializable]
        public class ScrollRectEvent : HelixEvent<Vector2>
        {
        }

        ScrollRectEvent m_OnValueChangedEvent;
        bool m_Hooked;

        public ScrollRectEvent onValueChanged
        {
            get
            {
                if (m_OnValueChangedEvent == null)
                    m_OnValueChangedEvent = NativeEvents.Bind(new ScrollRectEvent(), this, 0);
                if (!m_Hooked)
                {
                    m_Hooked = true;
                    onValueChangedNative += v => NativeEvents.Tell(m_OnValueChangedEvent, v);
                }
                return m_OnValueChangedEvent;
            }
            set
            {
                if (value != null)
                {
                    m_OnValueChangedEvent = NativeEvents.Bind(value, this, 0);
                    _ = onValueChanged;
                }
            }
        }
    }
}
