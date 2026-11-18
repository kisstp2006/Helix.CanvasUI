#pragma once

#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Transform/RectTransform.h>
#include <Helix/CanvasUI/Core/UIBehaviour.h>
#include <Helix/Engine/Event/Event.h>

// The rebuild loop of the canvas UI: elements registered for a layout or a graphic rebuild are rebuilt once a frame
// before the canvases batch (Canvas.willRenderCanvases): the layout queue sorted by depth in the hierarchy (parents
// first) through Prelayout, Layout and PostLayout, then the clippers cull, then the graphic queue through PreRender
// and LatePreRender. Elements of C# classes that are not components of C++ classes take part through their keys (the
// C# side keeps them and hears managedRebuild), in the same order as the others.
namespace helix::canvasui
{
    HX_ENUM(Namespace = "HelixEngine.CanvasUI")
    enum class CanvasUpdate : i32
    {
        Prelayout = 0,
        Layout = 1,
        PostLayout = 2,
        PreRender = 3,
        LatePreRender = 4,
        MaxUpdateValue = 5,
    };

    // What the rebuild loop drives.
    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class ICanvasElement
    {
    public:
        virtual ~ICanvasElement() = default;
        HX_FUNCTION() virtual void Rebuild(CanvasUpdate executing) = 0;
        HX_PROPERTY() virtual engine::Transform* transform() const = 0;
        HX_FUNCTION() virtual void LayoutComplete() = 0;
        HX_FUNCTION() virtual void GraphicUpdateComplete() = 0;
        HX_FUNCTION() virtual bool IsDestroyed() const = 0;
        // The engine object the element is (a component), so a destroyed one is never touched; null for others.
        virtual engine::Object* ElementObject() { return nullptr; }
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static)
    class CanvasUpdateRegistry
    {
    public:
        HX_FUNCTION(Internal, ApiName = "RegisterCanvasElementForLayoutRebuild") static void RegisterCanvasElementForLayoutRebuildNative(ICanvasElement* element);
        HX_FUNCTION(Internal, ApiName = "TryRegisterCanvasElementForLayoutRebuild")
        static bool TryRegisterCanvasElementForLayoutRebuildNative(ICanvasElement* element);
        HX_FUNCTION(Internal, ApiName = "RegisterCanvasElementForGraphicRebuild") static void RegisterCanvasElementForGraphicRebuildNative(ICanvasElement* element);
        HX_FUNCTION(Internal, ApiName = "TryRegisterCanvasElementForGraphicRebuild")
        static bool TryRegisterCanvasElementForGraphicRebuildNative(ICanvasElement* element);
        HX_FUNCTION(Internal, ApiName = "UnRegisterCanvasElementForRebuild") static void UnRegisterCanvasElementForRebuildNative(ICanvasElement* element);
        HX_FUNCTION(Internal, ApiName = "DisableCanvasElementForRebuild") static void DisableCanvasElementForRebuildNative(ICanvasElement* element);
        HX_FUNCTION() static bool IsRebuildingLayout();
        HX_FUNCTION() static bool IsRebuildingGraphics();

        // An element of a C# class (its key from the C# side, its transform for the order); queue 0 layout, 1 graphic.
        // 0: refused (inside that loop), 1: added, 2: already there.
        HX_FUNCTION(Internal) static i32 RegisterManaged(u64 key, engine::Transform* transform, i32 queue);
        // remove: 0 unregistered, 1 disabled; false when refused inside a loop.
        HX_FUNCTION(Internal) static bool UnregisterManaged(u64 key, bool disable);
        // The C# side's part of the loop: a stage (0 to 4) of the element of a key, 10 its layout complete, 11 its
        // graphic update complete, 12 asks whether it is destroyed (the answer through SetManagedDestroyed).
        HX_EVENT(Internal) static engine::Event<u64, i32>& managedRebuild();
        HX_FUNCTION(Internal) static void SetManagedDestroyed(bool destroyed);

        // The loop once now (the canvases run it before they batch).
        static void PerformUpdate();
    };
}
