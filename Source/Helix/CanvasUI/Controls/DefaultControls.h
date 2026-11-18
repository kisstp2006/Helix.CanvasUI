#pragma once

#include <Helix/Core/Containers/Array.h>
#include <Helix/Engine/Event/Event.h>
#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/Sprite/Sprite.h>
#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/Scripting/Types/Type.h>

// The controls' default objects (the menu's GameObject > UI items): each control's hierarchy with its components,
// sizes, anchors, sprites and colors. The objects are made by a factory (the default one makes plain GameObjects; an
// editor's adds undo). The controls with text (a button's label, a toggle's, the text, the input field, the dropdown)
// come with the text components.
namespace helix::canvasui
{
    // What makes the controls' GameObjects: a GameObject of the name with the components.
    class IFactoryControls
    {
    public:
        virtual ~IFactoryControls() = default;
        virtual engine::GameObject* CreateGameObject(StringView name, Span<const scripting::TypeId> components) = 0;
    };

    // The sprites of the controls across the bindings (C# has the struct DefaultControls.Resources over it).
    HX_STRUCT(Namespace = "HelixEngine.CanvasUI", Internal)
    struct DefaultControlsResourcesData
    {
        HX_FIELD() engine::EntityId standard;
        HX_FIELD() engine::EntityId background;
        HX_FIELD() engine::EntityId inputField;
        HX_FIELD() engine::EntityId knob;
        HX_FIELD() engine::EntityId checkmark;
        HX_FIELD() engine::EntityId dropdown;
        HX_FIELD() engine::EntityId mask;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static)
    class DefaultControls
    {
    public:
        struct Resources
        {
            engine::Sprite* standard = nullptr;
            engine::Sprite* background = nullptr;
            engine::Sprite* inputField = nullptr;
            engine::Sprite* knob = nullptr;
            engine::Sprite* checkmark = nullptr;
            engine::Sprite* dropdown = nullptr;
            engine::Sprite* mask = nullptr;

            static Resources From(const DefaultControlsResourcesData& data);
        };

        // The factory in use (the default one when none is set; null sets the default again).
        static IFactoryControls* factory();
        static void SetFactory(IFactoryControls* value);

        static engine::GameObject* CreatePanel(const Resources& resources);
        static engine::GameObject* CreateButton(const Resources& resources);
        static engine::GameObject* CreateImage(const Resources& resources);
        static engine::GameObject* CreateRawImage(const Resources& resources);
        static engine::GameObject* CreateSlider(const Resources& resources);
        static engine::GameObject* CreateScrollbar(const Resources& resources);
        static engine::GameObject* CreateToggle(const Resources& resources);
        static engine::GameObject* CreateScrollView(const Resources& resources);
        static engine::GameObject* CreateText(const Resources& resources);
        static engine::GameObject* CreateInputField(const Resources& resources);
        static engine::GameObject* CreateDropdown(const Resources& resources);

        // For C#: the controls by resources data, and a factory of C# (the native side asks it through the event, which
        // reads the name and the types being made and answers the GameObject).
        HX_FUNCTION(Internal) static engine::GameObject* CreatePanelInternal(DefaultControlsResourcesData resources) { return CreatePanel(Resources::From(resources)); }
        HX_FUNCTION(Internal) static engine::GameObject* CreateButtonInternal(DefaultControlsResourcesData resources) { return CreateButton(Resources::From(resources)); }
        HX_FUNCTION(Internal) static engine::GameObject* CreateImageInternal(DefaultControlsResourcesData resources) { return CreateImage(Resources::From(resources)); }
        HX_FUNCTION(Internal) static engine::GameObject* CreateRawImageInternal(DefaultControlsResourcesData resources)
        {
            return CreateRawImage(Resources::From(resources));
        }
        HX_FUNCTION(Internal) static engine::GameObject* CreateSliderInternal(DefaultControlsResourcesData resources) { return CreateSlider(Resources::From(resources)); }
        HX_FUNCTION(Internal) static engine::GameObject* CreateScrollbarInternal(DefaultControlsResourcesData resources)
        {
            return CreateScrollbar(Resources::From(resources));
        }
        HX_FUNCTION(Internal) static engine::GameObject* CreateToggleInternal(DefaultControlsResourcesData resources) { return CreateToggle(Resources::From(resources)); }
        HX_FUNCTION(Internal) static engine::GameObject* CreateScrollViewInternal(DefaultControlsResourcesData resources)
        {
            return CreateScrollView(Resources::From(resources));
        }
        HX_FUNCTION(Internal) static void SetManagedFactoryInternal(bool on);
        HX_FUNCTION(Internal) static engine::GameObject* CreateTextInternal(DefaultControlsResourcesData resources) { return CreateText(Resources::From(resources)); }
        HX_FUNCTION(Internal) static engine::GameObject* CreateInputFieldInternal(DefaultControlsResourcesData resources)
        {
            return CreateInputField(Resources::From(resources));
        }
        HX_FUNCTION(Internal) static engine::GameObject* CreateDropdownInternal(DefaultControlsResourcesData resources)
        {
            return CreateDropdown(Resources::From(resources));
        }
        HX_EVENT(Internal) static engine::Event<>& managedCreate();
        HX_FUNCTION(Internal) static String CreatingNameInternal();
        HX_FUNCTION(Internal) static i32 CreatingTypeCountInternal();
        HX_FUNCTION(Internal) static scripting::TypeId CreatingTypeInternal(i32 index);
        HX_FUNCTION(Internal) static void SetCreatedInternal(engine::GameObject* created);
    };

    // The text controls' default objects (the same hierarchies as DefaultControls' text controls, with the text
    // component's names): the text, a button, an input field, a dropdown and a scrollbar.
    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static)
    class TextDefaultControls
    {
    public:
        using Resources = DefaultControls::Resources;

        static engine::GameObject* CreateText(const Resources& resources);
        static engine::GameObject* CreateButton(const Resources& resources);
        static engine::GameObject* CreateInputField(const Resources& resources);
        static engine::GameObject* CreateDropdown(const Resources& resources);
        static engine::GameObject* CreateScrollbar(const Resources& resources);

        HX_FUNCTION(Internal) static engine::GameObject* CreateTextInternal(DefaultControlsResourcesData resources) { return CreateText(Resources::From(resources)); }
        HX_FUNCTION(Internal) static engine::GameObject* CreateButtonInternal(DefaultControlsResourcesData resources)
        {
            return CreateButton(Resources::From(resources));
        }
        HX_FUNCTION(Internal) static engine::GameObject* CreateInputFieldInternal(DefaultControlsResourcesData resources)
        {
            return CreateInputField(Resources::From(resources));
        }
        HX_FUNCTION(Internal) static engine::GameObject* CreateDropdownInternal(DefaultControlsResourcesData resources)
        {
            return CreateDropdown(Resources::From(resources));
        }
        HX_FUNCTION(Internal) static engine::GameObject* CreateScrollbarInternal(DefaultControlsResourcesData resources)
        {
            return CreateScrollbar(Resources::From(resources));
        }
    };
}
