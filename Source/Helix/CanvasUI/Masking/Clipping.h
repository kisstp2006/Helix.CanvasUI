#pragma once

#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Transform/RectTransform.h>
#include <Helix/CanvasUI/Core/UIBehaviour.h>
#include <Helix/Engine/Rendering/Pipeline/RenderStates.h>
#include <Helix/Math/Geometry.h>

namespace helix::engine
{
    class Material;
    class RectTransform;
    class Transform;
}

// Clipping and masking: RectMask2D clips the graphics under it to its rectangle (the clippers run after the layout,
// cutting with every RectMask2D above them); Mask masks through the stencil (each Mask a bit of depth); stencil
// materials are shared by their settings.
namespace helix::canvasui
{
    class IClippable;
    class RectMask2D;

    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class IClipper
    {
    public:
        virtual ~IClipper() = default;
        HX_FUNCTION() virtual void PerformClipping() = 0;
        // The engine object the clipper is (null for others).
        virtual engine::Object* ClipperObject() { return nullptr; }
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static)
    class ClipperRegistry
    {
    public:
        HX_FUNCTION() static void Register(IClipper* c);
        HX_FUNCTION() static void Unregister(IClipper* c);
        HX_FUNCTION() static void Disable(IClipper* c);
        // Every enabled clipper's clipping (the rebuild loop calls it after the layout).
        HX_FUNCTION() static void Cull();
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static)
    class Clipping
    {
    public:
        // The intersection of the masks' rectangles in their canvas's space; validRect false when it is empty (or
        // there are none).
        HX_FUNCTION(Out = "validRect") static Rect FindCullAndClipWorldRect(Span<RectMask2D* const> rectMaskParents, bool& validRect);
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static)
    class MaskUtilities
    {
    public:
        // The graphics under the mask hear that its state changed.
        HX_FUNCTION() static void Notify2DMaskStateChanged(engine::Component* mask);
        HX_FUNCTION() static void NotifyStencilStateChanged(engine::Component* mask);
        // The topmost canvas above (or at) start, stopping at a canvas with its own sorting.
        HX_FUNCTION() static engine::Transform* FindRootSortOverrideCanvas(engine::Transform* start);
        // The count of enabled masks between the transform and stopAfter.
        HX_FUNCTION() static i32 GetStencilDepth(engine::Transform* transform, engine::Transform* stopAfter);
        HX_FUNCTION() static bool IsDescendantOrSelf(engine::Transform* father, engine::Transform* child);
        // The nearest RectMask2D above the clippable (in its canvas).
        HX_FUNCTION() static RectMask2D* GetRectMaskForClippable(IClippable* clippable);
        HX_FUNCTION(Internal) static Array<RectMask2D*> GetRectMasksForClipInternal(RectMask2D* clipper);
    };

    // Materials with stencil settings, made once per settings and base material, counted by their users.
    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static)
    class StencilMaterial
    {
    public:
        HX_FUNCTION() static engine::Material* Add(engine::Material* baseMat, i32 stencilID, engine::StencilOp operation, engine::CompareFunction compareFunction,
                                                  engine::ColorWriteMask colorWriteMask);
        HX_FUNCTION() static engine::Material* Add(engine::Material* baseMat, i32 stencilID, engine::StencilOp operation, engine::CompareFunction compareFunction,
                                                  engine::ColorWriteMask colorWriteMask, i32 readMask, i32 writeMask);
        HX_FUNCTION() static void Remove(engine::Material* customMat);
        HX_FUNCTION() static void ClearAll();
        // How many stencil materials live (tests).
        static u32 Count();
    };
}
