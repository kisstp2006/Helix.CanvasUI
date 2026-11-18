using System;
using HelixEngine.Internal;

namespace HelixEngine.CanvasUI
{
    public static partial class DefaultControls
    {
        /// <summary>What makes the controls' GameObjects (an editor's factory adds undo).</summary>
        public interface IFactoryControls
        {
            GameObject CreateGameObject(string name, params Type[] components);
        }

        sealed class DefaultRuntimeFactory : IFactoryControls
        {
            public static readonly IFactoryControls Default = new DefaultRuntimeFactory();

            public GameObject CreateGameObject(string name, params Type[] components)
            {
                var go = new GameObject(name);
                foreach (Type t in components)
                    go.AddComponent(t);
                return go;
            }
        }

        static IFactoryControls m_CurrentFactory = DefaultRuntimeFactory.Default;
        static bool s_Hooked;

        /// <summary>The factory in use: the default one makes plain GameObjects natively; another is asked from the
        /// native side for every object.</summary>
        public static IFactoryControls factory
        {
            get => m_CurrentFactory;
            set
            {
                m_CurrentFactory = value ?? DefaultRuntimeFactory.Default;
                bool custom = m_CurrentFactory != DefaultRuntimeFactory.Default;
                if (custom && !s_Hooked)
                {
                    s_Hooked = true;
                    managedCreate += () =>
                    {
                        var types = new Type[CreatingTypeCountInternal()];
                        for (int i = 0; i < types.Length; i++)
                            types[i] = CreatingTypeInternal(i);
                        SetCreatedInternal(m_CurrentFactory.CreateGameObject(CreatingNameInternal(), types));
                    };
                }
                SetManagedFactoryInternal(custom);
            }
        }

        public struct Resources
        {
            public Sprite standard;
            public Sprite background;
            public Sprite inputField;
            public Sprite knob;
            public Sprite checkmark;
            public Sprite dropdown;
            public Sprite mask;

            internal DefaultControlsResourcesData ToData() => new DefaultControlsResourcesData
            {
                standard = Navigation.IdOf(standard),
                background = Navigation.IdOf(background),
                inputField = Navigation.IdOf(inputField),
                knob = Navigation.IdOf(knob),
                checkmark = Navigation.IdOf(checkmark),
                dropdown = Navigation.IdOf(dropdown),
                mask = Navigation.IdOf(mask),
            };
        }

        public static GameObject CreatePanel(Resources resources) => CreatePanelInternal(resources.ToData());
        public static GameObject CreateButton(Resources resources) => CreateButtonInternal(resources.ToData());
        public static GameObject CreateImage(Resources resources) => CreateImageInternal(resources.ToData());
        public static GameObject CreateRawImage(Resources resources) => CreateRawImageInternal(resources.ToData());
        public static GameObject CreateSlider(Resources resources) => CreateSliderInternal(resources.ToData());
        public static GameObject CreateScrollbar(Resources resources) => CreateScrollbarInternal(resources.ToData());
        public static GameObject CreateToggle(Resources resources) => CreateToggleInternal(resources.ToData());
        public static GameObject CreateScrollView(Resources resources) => CreateScrollViewInternal(resources.ToData());
        public static GameObject CreateText(Resources resources) => CreateTextInternal(resources.ToData());
        public static GameObject CreateInputField(Resources resources) => CreateInputFieldInternal(resources.ToData());
        public static GameObject CreateDropdown(Resources resources) => CreateDropdownInternal(resources.ToData());
    }

    public static partial class TextDefaultControls
    {
        public struct Resources
        {
            public Sprite standard;
            public Sprite background;
            public Sprite inputField;
            public Sprite knob;
            public Sprite checkmark;
            public Sprite dropdown;
            public Sprite mask;

            internal DefaultControlsResourcesData ToData() => new DefaultControls.Resources
            {
                standard = standard,
                background = background,
                inputField = inputField,
                knob = knob,
                checkmark = checkmark,
                dropdown = dropdown,
                mask = mask,
            }.ToData();
        }

        public static GameObject CreateText(Resources resources) => CreateTextInternal(resources.ToData());
        public static GameObject CreateButton(Resources resources) => CreateButtonInternal(resources.ToData());
        public static GameObject CreateInputField(Resources resources) => CreateInputFieldInternal(resources.ToData());
        public static GameObject CreateDropdown(Resources resources) => CreateDropdownInternal(resources.ToData());
        public static GameObject CreateScrollbar(Resources resources) => CreateScrollbarInternal(resources.ToData());
    }
}
