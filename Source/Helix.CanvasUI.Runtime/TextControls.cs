using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using HelixEngine.Events;
using HelixEngine.Internal;
using HelixEngine.Text;

namespace HelixEngine.Text
{
    public abstract partial class InputValidator
    {
        /// <summary>The character to insert (0 rejects it); the validator may change the text and the position.</summary>
        public abstract char Validate(ref string text, ref int pos, char ch);
    }
}

namespace HelixEngine.CanvasUI
{
    internal static class TextControlsHooks
    {
#pragma warning disable CA2255
        [ModuleInitializer]
#pragma warning restore CA2255
        internal static void Init() => ScriptBridge.WhenBindingsLoaded(Start);

        static void Start()
        {
            TextInput.validateCharacter += (input, pos, ch) => input?.ValidateManaged(pos, ch);
            Dropdown.managedOptions += d => d?.PushOptions();
        }
    }

    public partial class TextInput
    {
        [Serializable]
        public class SubmitEvent : HelixEvent<string>
        {
        }

        [Serializable]
        public class OnChangeEvent : HelixEvent<string>
        {
        }

        [Serializable]
        public class SelectionEvent : HelixEvent<string>
        {
        }

        [Serializable]
        public class TextSelectionEvent : HelixEvent<string, int, int>
        {
        }

        /// <summary>The character to insert at charIndex (0 rejects it).</summary>
        public delegate char OnValidateInput(string text, int charIndex, char addedChar);

        OnChangeEvent m_OnValueChanged;
        SubmitEvent m_OnEndEdit, m_OnSubmit;
        SelectionEvent m_OnSelect, m_OnDeselect;
        TextSelectionEvent m_OnTextSelection, m_OnEndTextSelection;
        OnValidateInput m_OnValidateInput;
        readonly bool[] m_Hooked = new bool[7];

        // The C# event over the native one of the index (bound once, told by the native listener).
        T Event<T>(ref T field, int index, Action<T> hook) where T : HelixEventBase, new()
        {
            if (field == null)
                field = NativeEvents.Bind(new T(), this, index);
            if (!m_Hooked[index])
            {
                m_Hooked[index] = true;
                hook(field);
            }
            return field;
        }

        void SetEvent<T>(ref T field, T value, int index, Action<T> hook) where T : HelixEventBase, new()
        {
            if (value == null)
                return;
            field = NativeEvents.Bind(value, this, index);
            Event(ref field, index, hook);
        }

        void HookValueChanged(OnChangeEvent e) => onValueChangedNative += v => NativeEvents.Tell(m_OnValueChanged, v);
        void HookEndEdit(SubmitEvent e) => onEndEditNative += v => NativeEvents.Tell(m_OnEndEdit, v);
        void HookSubmit(SubmitEvent e) => onSubmitNative += v => NativeEvents.Tell(m_OnSubmit, v);
        void HookSelect(SelectionEvent e) => onSelectNative += v => NativeEvents.Tell(m_OnSelect, v);
        void HookDeselect(SelectionEvent e) => onDeselectNative += v => NativeEvents.Tell(m_OnDeselect, v);
        void HookTextSelection(TextSelectionEvent e) => onTextSelectionNative += (v, a, b) => NativeEvents.Tell(m_OnTextSelection, v, a, b);
        void HookEndTextSelection(TextSelectionEvent e) => onEndTextSelectionNative += (v, a, b) => NativeEvents.Tell(m_OnEndTextSelection, v, a, b);

        public OnChangeEvent onValueChanged
        {
            get => Event(ref m_OnValueChanged, 0, HookValueChanged);
            set => SetEvent(ref m_OnValueChanged, value, 0, HookValueChanged);
        }

        public SubmitEvent onEndEdit
        {
            get => Event(ref m_OnEndEdit, 1, HookEndEdit);
            set => SetEvent(ref m_OnEndEdit, value, 1, HookEndEdit);
        }

        public SubmitEvent onSubmit
        {
            get => Event(ref m_OnSubmit, 2, HookSubmit);
            set => SetEvent(ref m_OnSubmit, value, 2, HookSubmit);
        }

        public SelectionEvent onSelect
        {
            get => Event(ref m_OnSelect, 3, HookSelect);
            set => SetEvent(ref m_OnSelect, value, 3, HookSelect);
        }

        public SelectionEvent onDeselect
        {
            get => Event(ref m_OnDeselect, 4, HookDeselect);
            set => SetEvent(ref m_OnDeselect, value, 4, HookDeselect);
        }

        public TextSelectionEvent onTextSelection
        {
            get => Event(ref m_OnTextSelection, 5, HookTextSelection);
            set => SetEvent(ref m_OnTextSelection, value, 5, HookTextSelection);
        }

        public TextSelectionEvent onEndTextSelection
        {
            get => Event(ref m_OnEndTextSelection, 6, HookEndTextSelection);
            set => SetEvent(ref m_OnEndTextSelection, value, 6, HookEndTextSelection);
        }

        /// <summary>Takes the place of the character validation while set.</summary>
        public OnValidateInput onValidateInput
        {
            get => m_OnValidateInput;
            set
            {
                m_OnValidateInput = value;
                SetManagedValidationInternal(value != null);
            }
        }

        /// <summary>The validator of CharacterValidation.CustomValidator (setting one selects it).</summary>
        public InputValidator inputValidator
        {
            get => inputValidatorInternal;
            set => inputValidatorInternal = value;
        }

        // A character typed (the native side's validation already applied unless onValidateInput is set).
        internal void ValidateManaged(int pos, int ch)
        {
            if (m_OnValidateInput != null)
            {
                SetValidatedCharacterInternal(m_OnValidateInput(text, pos, (char)ch));
                return;
            }
            InputValidator validator = inputValidatorInternal;
            if (characterValidation != CharacterValidation.CustomValidator || validator == null)
                return;
            string t = text;
            int p = pos;
            char result = validator.Validate(ref t, ref p, (char)ch);
            SetValidatedInternal(result, t ?? string.Empty, p);
        }
    }

    public partial class Dropdown
    {
        [Serializable]
        public class DropdownEvent : HelixEvent<int>
        {
        }

        [Serializable]
        public class OptionData
        {
            [SerializeField] string m_Text;
            [SerializeField] Sprite m_Image;
            [SerializeField] Color m_Color = Color.white;

            public string text { get => m_Text; set => m_Text = value; }
            public Sprite image { get => m_Image; set => m_Image = value; }
            public Color color { get => m_Color; set => m_Color = value; }

            public OptionData()
            {
            }

            public OptionData(string text) => m_Text = text;

            public OptionData(Sprite image) => m_Image = image;

            public OptionData(string text, Sprite image, Color color)
            {
                m_Text = text;
                m_Image = image;
                m_Color = color;
            }
        }

        [Serializable]
        public class OptionDataList
        {
            [SerializeField] List<OptionData> m_Options = new List<OptionData>();

            public List<OptionData> options { get => m_Options; set => m_Options = value; }
        }

        DropdownEvent m_OnValueChangedEvent;
        bool m_Hooked;
        OptionDataList m_Options;

        public DropdownEvent onValueChanged
        {
            get
            {
                if (m_OnValueChangedEvent == null)
                    m_OnValueChangedEvent = NativeEvents.Bind(new DropdownEvent(), this, 0);
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

        // The list of C# (read once from the native options); the native side pulls it in before it reads them.
        OptionDataList Options()
        {
            if (m_Options == null)
            {
                m_Options = new OptionDataList();
                int count = OptionCountInternal();
                for (int i = 0; i < count; i++)
                {
                    DropdownOptionData d = OptionDataInternal(i);
                    m_Options.options.Add(new OptionData(OptionTextInternal(i), Resources.EntityIdToObject(d.image) as Sprite, d.color));
                }
                SetManagedOptionsInternal(true);
            }
            return m_Options;
        }

        internal void PushOptions()
        {
            if (m_Options == null)
                return;
            ClearOptionsInternal();
            foreach (OptionData o in m_Options.options)
            {
                if (o == null)
                    continue;
                AddOptionInternal(o.text ?? string.Empty, new DropdownOptionData
                {
                    image = o.image != null ? o.image.GetEntityId() : default,
                    color = o.color,
                });
            }
        }

        public List<OptionData> options
        {
            get => Options().options;
            set
            {
                Options().options = value ?? new List<OptionData>();
                RefreshShownValue();
            }
        }

        public void AddOptions(List<OptionData> options)
        {
            Options().options.AddRange(options);
            RefreshShownValue();
        }

        public void AddOptions(List<string> options)
        {
            List<OptionData> list = Options().options;
            foreach (string s in options)
                list.Add(new OptionData(s));
            RefreshShownValue();
        }

        public void AddOptions(List<Sprite> options)
        {
            List<OptionData> list = Options().options;
            foreach (Sprite s in options)
                list.Add(new OptionData(s));
            RefreshShownValue();
        }

        public void ClearOptions()
        {
            Options().options.Clear();
            ClearOptionsNative();
        }
    }
}
