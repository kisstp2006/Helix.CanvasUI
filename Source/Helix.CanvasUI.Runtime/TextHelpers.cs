using System;
using System.Collections.Generic;
using HelixEngine.CanvasUI;

namespace HelixEngine.Text
{
    public static partial class TextUtilities
    {
        /// <summary>The reference's hash of a name, case insensitive (the tags' and assets' lookups).</summary>
        public static int GetHashCode(string s)
        {
            int hash = 0;
            foreach (char c in s ?? "")
                hash = ((hash << 5) + hash) ^ ToUpperFast(c);
            return hash;
        }

        public static int GetSimpleHashCode(string s)
        {
            int hash = 0;
            foreach (char c in s ?? "")
                hash = ((hash << 5) + hash) ^ c;
            return hash;
        }

        public static uint GetHashCodeCaseInSensitive(string s) => (uint)GetHashCode(s);

        public static uint GetSimpleHashCodeLowercase(string s)
        {
            uint hash = 5381;
            foreach (char c in s ?? "")
                hash = (hash << 5) + hash ^ ToLowerFast(c);
            return hash;
        }

        public static int HexToInt(char hex) => hex switch
        {
            >= '0' and <= '9' => hex - '0',
            >= 'a' and <= 'f' => hex - 'a' + 10,
            >= 'A' and <= 'F' => hex - 'A' + 10,
            _ => 15,
        };

        public static int StringHexToInt(string s)
        {
            int value = 0;
            foreach (char c in s ?? "")
                value = value * 16 + HexToInt(c);
            return value;
        }

        public static char ToLowerFast(char c) => c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
        public static char ToUpperFast(char c) => c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
    }

    public class TextParsingUtilities
    {
        public static TextParsingUtilities instance { get; } = new TextParsingUtilities();

        public static uint GetHashCode(string s)
        {
            uint hash = 0;
            foreach (char c in s ?? "")
                hash = ((hash << 5) + hash) ^ ToUpperASCIIFast(c);
            return hash;
        }

        public static uint GetHashCodeCaseSensitive(string s)
        {
            uint hash = 0;
            foreach (char c in s ?? "")
                hash = ((hash << 5) + hash) ^ c;
            return hash;
        }

        public static char ToLowerASCIIFast(char c) => c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
        public static uint ToLowerASCIIFast(uint c) => c >= 'A' && c <= 'Z' ? c + 32 : c;
        public static char ToUpperASCIIFast(char c) => c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
        public static uint ToUpperASCIIFast(uint c) => c >= 'a' && c <= 'z' ? c - 32 : c;
        public static uint ToUpperFast(uint c) => char.IsLetter((char)c) && c < 0x10000 ? char.ToUpperInvariant((char)c) : c;
        public static bool IsHighSurrogate(uint c) => c >= 0xD800 && c <= 0xDBFF;
        public static bool IsLowSurrogate(uint c) => c >= 0xDC00 && c <= 0xDFFF;
        public static uint ConvertToUTF32(uint highSurrogate, uint lowSurrogate) => ((highSurrogate - 0xD800) << 10) + (lowSurrogate - 0xDC00) + 0x10000;
    }

    /// <summary>Texts to update before the canvases draw (the rebuild loop does it for every dirty text).</summary>
    public class UpdateManager
    {
        public static void RegisterTextObjectForUpdate(TextBase textObject)
        {
            if (textObject != null)
                textObject.havePropertiesChanged = true;
        }

        public static void UnRegisterTextObjectForUpdate(TextBase textObject)
        {
        }

        public static void RegisterTextElementForLayoutRebuild(TextBase element)
        {
            if (element != null)
                element.SetLayoutDirty();
        }

        public static void RegisterTextElementForGraphicRebuild(TextBase element)
        {
            if (element != null)
                element.SetVerticesDirty();
        }

        public static void RegisterTextElementForCullingUpdate(TextBase element)
        {
            if (element != null)
                element.SetMaterialDirty();
        }

        public static void UnRegisterTextElementForRebuild(TextBase element)
        {
        }
    }

    public class UpdateRegistry
    {
        public static bool RegisterCanvasElementForLayoutRebuild(ICanvasElement element) => CanvasUpdateRegistry.TryRegisterCanvasElementForLayoutRebuild(element);
        public static bool RegisterCanvasElementForGraphicRebuild(ICanvasElement element) => CanvasUpdateRegistry.TryRegisterCanvasElementForGraphicRebuild(element);
        public static void UnRegisterCanvasElementForRebuild(ICanvasElement element) => CanvasUpdateRegistry.UnRegisterCanvasElementForRebuild(element);
    }

    /// <summary>A list of callbacks called in order (the event manager's events).</summary>
    public class FastAction
    {
        readonly List<Action> m_Delegates = new List<Action>();

        public void Add(Action rhs)
        {
            if (!m_Delegates.Contains(rhs))
                m_Delegates.Add(rhs);
        }

        public void Remove(Action rhs) => m_Delegates.Remove(rhs);

        public void Call()
        {
            foreach (Action a in m_Delegates.ToArray())
                a();
        }
    }

    public class FastAction<A>
    {
        readonly List<Action<A>> m_Delegates = new List<Action<A>>();

        public void Add(Action<A> rhs)
        {
            if (!m_Delegates.Contains(rhs))
                m_Delegates.Add(rhs);
        }

        public void Remove(Action<A> rhs) => m_Delegates.Remove(rhs);

        public void Call(A a)
        {
            foreach (Action<A> d in m_Delegates.ToArray())
                d(a);
        }
    }

    public class FastAction<A, B>
    {
        readonly List<Action<A, B>> m_Delegates = new List<Action<A, B>>();

        public void Add(Action<A, B> rhs)
        {
            if (!m_Delegates.Contains(rhs))
                m_Delegates.Add(rhs);
        }

        public void Remove(Action<A, B> rhs) => m_Delegates.Remove(rhs);

        public void Call(A a, B b)
        {
            foreach (Action<A, B> d in m_Delegates.ToArray())
                d(a, b);
        }
    }

    public class FastAction<A, B, C>
    {
        readonly List<Action<A, B, C>> m_Delegates = new List<Action<A, B, C>>();

        public void Add(Action<A, B, C> rhs)
        {
            if (!m_Delegates.Contains(rhs))
                m_Delegates.Add(rhs);
        }

        public void Remove(Action<A, B, C> rhs) => m_Delegates.Remove(rhs);

        public void Call(A a, B b, C c)
        {
            foreach (Action<A, B, C> d in m_Delegates.ToArray())
                d(a, b, c);
        }
    }

    /// <summary>The text system's events: changed materials, fonts, sprites and gradients, and every text's new mesh.</summary>
    public static class TextEventManager
    {
        public static readonly FastAction<bool, Material> MATERIAL_PROPERTY_EVENT = new FastAction<bool, Material>();
        public static readonly FastAction<bool, Object> FONT_PROPERTY_EVENT = new FastAction<bool, Object>();
        public static readonly FastAction<bool, Object> SPRITE_ASSET_PROPERTY_EVENT = new FastAction<bool, Object>();
        public static readonly FastAction<bool, Object> TEXTMESHPRO_PROPERTY_EVENT = new FastAction<bool, Object>();
        public static readonly FastAction<GameObject, Material, Material> DRAG_AND_DROP_MATERIAL_EVENT = new FastAction<GameObject, Material, Material>();
        public static readonly FastAction<bool> TEXT_STYLE_PROPERTY_EVENT = new FastAction<bool>();
        public static readonly FastAction<Object> COLOR_GRADIENT_PROPERTY_EVENT = new FastAction<Object>();
        public static readonly FastAction TMP_SETTINGS_PROPERTY_EVENT = new FastAction();
        public static readonly FastAction RESOURCE_LOAD_EVENT = new FastAction();
        public static readonly FastAction<bool, Object> TEXTMESHPRO_UGUI_PROPERTY_EVENT = new FastAction<bool, Object>();
        public static readonly FastAction<Object> TEXT_CHANGED_EVENT = new FastAction<Object>();

        public static void ON_MATERIAL_PROPERTY_CHANGED(bool isChanged, Material mat) => MATERIAL_PROPERTY_EVENT.Call(isChanged, mat);
        public static void ON_FONT_PROPERTY_CHANGED(bool isChanged, Object obj) => FONT_PROPERTY_EVENT.Call(isChanged, obj);
        public static void ON_SPRITE_ASSET_PROPERTY_CHANGED(bool isChanged, Object obj) => SPRITE_ASSET_PROPERTY_EVENT.Call(isChanged, obj);
        public static void ON_TEXTMESHPRO_PROPERTY_CHANGED(bool isChanged, Object obj) => TEXTMESHPRO_PROPERTY_EVENT.Call(isChanged, obj);
        public static void ON_DRAG_AND_DROP_MATERIAL_CHANGED(GameObject sender, Material currentMaterial, Material newMaterial) =>
            DRAG_AND_DROP_MATERIAL_EVENT.Call(sender, currentMaterial, newMaterial);
        public static void ON_TEXT_STYLE_PROPERTY_CHANGED(bool isChanged) => TEXT_STYLE_PROPERTY_EVENT.Call(isChanged);
        public static void ON_COLOR_GRADIENT_PROPERTY_CHANGED(Object gradient) => COLOR_GRADIENT_PROPERTY_EVENT.Call(gradient);
        public static void ON_TEXT_CHANGED(Object obj) => TEXT_CHANGED_EVENT.Call(obj);
        public static void ON_TMP_SETTINGS_CHANGED() => TMP_SETTINGS_PROPERTY_EVENT.Call();
        public static void ON_RESOURCES_LOADED() => RESOURCE_LOAD_EVENT.Call();
        public static void ON_TEXTMESHPRO_UGUI_PROPERTY_CHANGED(bool isChanged, Object obj) => TEXTMESHPRO_UGUI_PROPERTY_EVENT.Call(isChanged, obj);
    }

    /// <summary>The text system's numbers.</summary>
    public static class Math
    {
        public const float FLOAT_MAX = 32767;
        public const float FLOAT_MIN = -32767;
        public const int INT_MAX = 2147483647;
        public const int INT_MIN = -2147483647;
        public const float FLOAT_UNSET = -32767;
        public const int INT_UNSET = -32767;
        public static Vector2 MAX_16BIT = new Vector2(32767, 32767);
        public static Vector2 MIN_16BIT = new Vector2(-32767, -32767);

        public static bool Approximately(float a, float b) => b - 0.0001f < a && a < b + 0.0001f;
        public static bool Equals(float a, float b) => System.Math.Abs(a - b) < 1e-6f;
    }

    /// <summary>Insets of a highlight or a margin (left, right, top, bottom).</summary>
    public struct Offset : IEquatable<Offset>
    {
        public float left;
        public float right;
        public float top;
        public float bottom;

        public float horizontal
        {
            get => left;
            set
            {
                left = value;
                right = value;
            }
        }

        public float vertical
        {
            get => top;
            set
            {
                top = value;
                bottom = value;
            }
        }

        public static Offset zero => new Offset();

        public Offset(float left, float right, float top, float bottom)
        {
            this.left = left;
            this.right = right;
            this.top = top;
            this.bottom = bottom;
        }

        public Offset(float horizontal, float vertical) : this(horizontal, horizontal, vertical, vertical)
        {
        }

        public bool Equals(Offset other) => left == other.left && right == other.right && top == other.top && bottom == other.bottom;
        public override bool Equals(object obj) => obj is Offset o && Equals(o);
        public override int GetHashCode() => HashCode.Combine(left, right, top, bottom);
        public static bool operator ==(Offset lhs, Offset rhs) => lhs.Equals(rhs);
        public static bool operator !=(Offset lhs, Offset rhs) => !lhs.Equals(rhs);
        public static Offset operator *(Offset a, float b) => new Offset(a.left * b, a.right * b, a.top * b, a.bottom * b);
    }

    public struct HighlightState : IEquatable<HighlightState>
    {
        public Color32 color;
        public Offset padding;

        public HighlightState(Color32 color, Offset padding)
        {
            this.color = color;
            this.padding = padding;
        }

        public bool Equals(HighlightState other) => color.Equals(other.color) && padding == other.padding;
        public override bool Equals(object obj) => obj is HighlightState h && Equals(h);
        public override int GetHashCode() => HashCode.Combine(color, padding);
        public static bool operator ==(HighlightState lhs, HighlightState rhs) => lhs.Equals(rhs);
        public static bool operator !=(HighlightState lhs, HighlightState rhs) => !lhs.Equals(rhs);
    }

    public enum TagUnitType
    {
        Pixels = 0,
        FontUnits = 1,
        Percentage = 2,
    }

    public enum TagValueType
    {
        None = 0x0,
        NumericalValue = 0x1,
        StringValue = 0x2,
        ColorValue = 0x4,
    }

    public enum MaskingOffsetMode
    {
        Percentage = 0,
        Pixel = 1,
    }

    public static class TextExtensionMethods
    {
        public static string ArrayToString(this char[] chars) => new string(chars ?? Array.Empty<char>());

        public static string IntToString(this int[] unicodes)
        {
            var s = new System.Text.StringBuilder();
            foreach (int u in unicodes ?? Array.Empty<int>())
                s.Append(char.ConvertFromUtf32(u));
            return s.ToString();
        }

        public static string IntToString(this int[] unicodes, int start, int length)
        {
            var s = new System.Text.StringBuilder();
            for (int i = start; i < start + length && i < (unicodes?.Length ?? 0); i++)
                s.Append(char.ConvertFromUtf32(unicodes[i]));
            return s.ToString();
        }

        public static int FindInstanceID<T>(this List<T> list, T target) where T : Object
        {
            for (int i = 0; i < list.Count; i++)
                if (list[i] == target)
                    return i;
            return -1;
        }

        public static bool Compare(this Color32 a, Color32 b) => a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        public static bool CompareRGB(this Color32 a, Color32 b) => a.r == b.r && a.g == b.g && a.b == b.b;
        public static bool Compare(this Color a, Color b) => a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        public static bool CompareRGB(this Color a, Color b) => a.r == b.r && a.g == b.g && a.b == b.b;
        public static Color32 Multiply(this Color32 c1, Color32 c2) =>
            new Color32((byte)(c1.r * c2.r / 255), (byte)(c1.g * c2.g / 255), (byte)(c1.b * c2.b / 255), (byte)(c1.a * c2.a / 255));
        public static Color32 Tint(this Color32 c1, Color32 c2) => c1.Multiply(c2);
        public static Color32 Tint(this Color32 c1, float tint) =>
            new Color32((byte)Mathf.Clamp(c1.r * tint, 0, 255), (byte)Mathf.Clamp(c1.g * tint, 0, 255), (byte)Mathf.Clamp(c1.b * tint, 0, 255),
                        (byte)Mathf.Clamp(c1.a * tint, 0, 255));
        public static bool Compare(this Vector3 v1, Vector3 v2, int accuracy) =>
            (int)(v1.x * accuracy) == (int)(v2.x * accuracy) && (int)(v1.y * accuracy) == (int)(v2.y * accuracy) && (int)(v1.z * accuracy) == (int)(v2.z * accuracy);
        public static bool Compare(this Quaternion q1, Quaternion q2, int accuracy) =>
            (int)(q1.x * accuracy) == (int)(q2.x * accuracy) && (int)(q1.y * accuracy) == (int)(q2.y * accuracy) && (int)(q1.z * accuracy) == (int)(q2.z * accuracy) &&
            (int)(q1.w * accuracy) == (int)(q2.w * accuracy);
    }

    public static class FontUtilities
    {
        /// <summary>The font asset (the font or one of its fallbacks) that has the character.</summary>
        public static FontAsset SearchForCharacter(FontAsset font, uint unicode, out Character character)
        {
            character = FontAssetUtilities.GetCharacterFromFontAsset(unicode, font, true, FontStyles.Normal, FontWeight.Regular, out bool _);
            return character?.textAsset as FontAsset;
        }

        public static FontAsset SearchForCharacter(List<FontAsset> fonts, uint unicode, out Character character)
        {
            character = null;
            foreach (FontAsset f in fonts ?? new List<FontAsset>())
            {
                FontAsset found = SearchForCharacter(f, unicode, out character);
                if (found != null)
                    return found;
            }
            return null;
        }
    }

    public static class ResourceManager
    {
        public static TextSettings GetTextSettings() => TextSettings.instance;

        public static void AddFontAsset(FontAsset fontAsset) => MaterialReferenceManager.AddFontAsset(fontAsset);
    }

    /// <summary>Plays the frames of an animated sprite (<sprite anim="first,last,fps">) on its text.</summary>
    public class SpriteAnimator : GameBehaviour
    {
        readonly Dictionary<int, bool> m_Animations = new Dictionary<int, bool>();

        public void StopAllAnimations()
        {
            StopAllCoroutines();
            m_Animations.Clear();
        }

        public void DoSpriteAnimation(int currentCharacter, SpriteAsset spriteAsset, int start, int end, int framerate)
        {
            if (m_Animations.ContainsKey(currentCharacter))
                return;
            m_Animations[currentCharacter] = true;
        }
    }

    /// <summary>The text area of a world text (obsolete: the RectTransform is the area).</summary>
    [Obsolete("The TextContainer is now obsolete. Use the RectTransform instead.")]
    public class TextContainer : GameBehaviour
    {
        public Vector2 anchorPosition => GetComponent<RectTransform>()?.anchoredPosition ?? Vector2.zero;
        public Rect rect => GetComponent<RectTransform>()?.rect ?? new Rect();
        public Vector2 size => rect.size;
        public float width => rect.width;
        public float height => rect.height;
        public bool isDefaultWidth => false;
        public bool isDefaultHeight => false;
        public bool isAutoFitting { get; set; }
        public Vector4 margins { get; set; }
    }
}
