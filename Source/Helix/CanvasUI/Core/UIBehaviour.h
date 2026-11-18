#pragma once

#include <Helix/Engine/Script/GameBehaviour.h>
#include <Helix/Scripting/Types/Type.h>

#include <type_traits>

// UIBehaviour: the base of the canvas UI's components. Its messages are protected virtual functions (C# classes
// deriving from the components override them and call the base); IsActive and IsDestroyed answer for the component.
namespace helix::canvasui
{
    class IClippable;
    class ICanvasRaycastFilterNative;
    class IMaterialModifier;
    class IMeshModifier;
    class ILayoutElement;
    class ILayoutController;
    class ILayoutIgnorer;
    class INativeEventHandler;

    HX_CLASS(Namespace = "HelixEngine.UIEvents", Abstract)
    class UIBehaviour : public engine::GameBehaviour
    {
    public:
        UIBehaviour() = default;
        virtual ~UIBehaviour() = default;

        // Active and enabled (the component takes part).
        HX_FUNCTION() virtual bool IsActive() const { return isActiveAndEnabled(); }
        // The component is gone (its C# object may live on).
        HX_FUNCTION() bool IsDestroyed() const;

        // What the component is besides its class (the components of C++ answer for their interfaces).
        virtual IClippable* AsClippable() { return nullptr; }
        virtual ICanvasRaycastFilterNative* AsRaycastFilter() { return nullptr; }
        virtual IMaterialModifier* AsMaterialModifier() { return nullptr; }
        virtual IMeshModifier* AsMeshModifier() { return nullptr; }
        virtual ILayoutElement* AsLayoutElement() { return nullptr; }
        virtual ILayoutController* AsLayoutController() { return nullptr; }
        virtual ILayoutIgnorer* AsLayoutIgnorer() { return nullptr; }
        virtual INativeEventHandler* AsEventHandler() { return nullptr; }

        HX_FUNCTION(Protected) virtual void Awake() {}
        HX_FUNCTION(Protected) virtual void OnEnable() {}
        HX_FUNCTION(Protected) virtual void Start() {}
        HX_FUNCTION(Protected) virtual void OnDisable() {}
        HX_FUNCTION(Protected) virtual void OnDestroy() {}
        HX_FUNCTION(Protected) virtual void OnValidate() {}
        HX_FUNCTION(Protected) virtual void Reset() {}
        HX_FUNCTION(Protected) virtual void OnRectTransformDimensionsChange() {}
        HX_FUNCTION(Protected) virtual void OnBeforeTransformParentChanged() {}
        HX_FUNCTION(Protected) virtual void OnTransformParentChanged() {}
        HX_FUNCTION(Protected) virtual void OnDidApplyAnimationProperties() {}
        HX_FUNCTION(Protected) virtual void OnCanvasGroupChanged() {}
        HX_FUNCTION(Protected) virtual void OnCanvasHierarchyChanged() {}
    };

    // The component of a script class or an engine class on a GameObject (null without one).
    template <class T>
    T* ComponentOf(const engine::GameObject* g)
    {
        if (!g)
            return nullptr;
        // Script classes (below GameBehaviour, whose class ID they inherit) by their type; engine classes by class.
        if constexpr (std::is_base_of_v<engine::GameBehaviour, T>)
            return static_cast<T*>(g->GetComponent(scripting::TypeOf<T>()));
        else
            return g->GetComponent<T>();
    }

    template <class T>
    T* ComponentOf(const engine::Component* c)
    {
        return c ? ComponentOf<T>(c->gameObject()) : nullptr;
    }

    // The component as a UIBehaviour of C++ (or of a C# class deriving from one); null for others.
    UIBehaviour* AsUIBehaviour(engine::Component* c);

    // Every component of the GameObject that is a UIBehaviour (in their order).
    void UIBehavioursOf(const engine::GameObject* g, Array<UIBehaviour*>& out);
}
