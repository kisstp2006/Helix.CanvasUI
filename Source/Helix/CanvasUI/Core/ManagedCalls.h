#pragma once

#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Transform/RectTransform.h>
#include <Helix/Engine/Event/Event.h>
#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Math/Geometry.h>

namespace helix::engine
{
    class Camera;
    class Material;
}

// The canvas UI's interfaces implemented by components of C# classes that are not components of C++ classes: C++
// asks the C# side through these events (the C# side looks the interface up on the component and answers through
// the setters). The components of C++ classes answer in C++.
namespace helix::canvasui
{
    class VertexHelper;

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static, Internal)
    class ManagedCalls
    {
    public:
        // IMaterialModifier.GetModifiedMaterial: the material in, the answer through SetMaterial (unchanged without one).
        HX_EVENT(Internal) static engine::Event<engine::Component*, engine::Material*>& modifyMaterial();
        // IMeshModifier.ModifyMesh.
        HX_EVENT(Internal) static engine::Event<engine::Component*, VertexHelper*>& modifyMesh();
        // ICanvasRaycastFilter.IsRaycastLocationValid: the answer through SetBool (true without the interface).
        HX_EVENT(Internal) static engine::Event<engine::Component*, Vector2, engine::Camera*>& raycastFilter();
        // Whether the component implements an interface (by its name in the canvas UI), through SetBool.
        HX_EVENT(Internal) static engine::Event<engine::Component*, StringView>& implements();
        // The layout's calls: 0 CalculateLayoutInputHorizontal, 1 CalculateLayoutInputVertical, 2 SetLayoutHorizontal,
        // 3 SetLayoutVertical; 10 to 15 the properties minWidth to flexibleHeight and 16 layoutPriority (through
        // SetFloat), 17 ignoreLayout (through SetBool).
        HX_EVENT(Internal) static engine::Event<engine::Component*, i32>& layout();
        HX_FUNCTION(Internal) static void SetMaterial(engine::Material* material);
        HX_FUNCTION(Internal) static void SetBool(bool value);
        HX_FUNCTION(Internal) static void SetFloat(f32 value);

        // For C++: the C# side's answers.
        static engine::Material* ModifyMaterial(engine::Component* c, engine::Material* material);
        static void ModifyMesh(engine::Component* c, VertexHelper& vh);
        static bool RaycastFilter(engine::Component* c, Vector2 sp, engine::Camera* camera, bool& implemented);
        static bool Implements(engine::Component* c, StringView interfaceName);
        static void LayoutCall(engine::Component* c, i32 call);
        static f32 LayoutFloat(engine::Component* c, i32 property);
        static bool LayoutBool(engine::Component* c, i32 property);
        // A component of a C# script class: the C# side answers for the interfaces its C++ base (if any) does not have.
        static bool IsManaged(const engine::Component* c);
    };
}
