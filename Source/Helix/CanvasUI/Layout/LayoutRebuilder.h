#pragma once

#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Transform/RectTransform.h>
#include <Helix/CanvasUI/Core/CanvasUpdateRegistry.h>
#include <Helix/CanvasUI/Core/OwnedRoot.h>

namespace helix::engine
{
    class RectTransform;
}

// The layout's interfaces and its rebuild: a layout root (the topmost RectTransform with an enabled layout group
// above the changed one) is rebuilt in the Layout stage: the elements' sizes calculated bottom up (children first),
// then the controllers set them top down (self controllers before the groups), horizontally first, then vertically.
namespace helix::canvasui
{
    // What a layout reads of an element.
    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class ILayoutElement
    {
    public:
        virtual ~ILayoutElement() = default;
        HX_FUNCTION() virtual void CalculateLayoutInputHorizontal() = 0;
        HX_FUNCTION() virtual void CalculateLayoutInputVertical() = 0;
        HX_PROPERTY() virtual f32 minWidth() const = 0;
        HX_PROPERTY() virtual f32 preferredWidth() const = 0;
        HX_PROPERTY() virtual f32 flexibleWidth() const = 0;
        HX_PROPERTY() virtual f32 minHeight() const = 0;
        HX_PROPERTY() virtual f32 preferredHeight() const = 0;
        HX_PROPERTY() virtual f32 flexibleHeight() const = 0;
        HX_PROPERTY() virtual i32 layoutPriority() const = 0;
    };

    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class ILayoutController
    {
    public:
        virtual ~ILayoutController() = default;
        HX_FUNCTION() virtual void SetLayoutHorizontal() = 0;
        HX_FUNCTION() virtual void SetLayoutVertical() = 0;
        // 1: a self controller (sets its own RectTransform), 2: a group (sets its children's), 0: neither.
        virtual i32 ControllerKind() const { return 0; }
        // Left out when its own layout runs over this RectTransform (a scroll rect's content).
        virtual bool SkipsLayoutOf(const engine::RectTransform* rect) const
        {
            (void)rect;
            return false;
        }
    };

    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class ILayoutGroup : public ILayoutController
    {
    public:
        i32 ControllerKind() const override { return 2; }
    };

    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class ILayoutSelfController : public ILayoutController
    {
    public:
        i32 ControllerKind() const override { return 1; }
    };

    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class ILayoutIgnorer
    {
    public:
        virtual ~ILayoutIgnorer() = default;
        HX_PROPERTY() virtual bool ignoreLayout() const = 0;
    };

    HX_ENUM(Namespace = "HelixEngine.CanvasUI", Internal)
    enum class LayoutProperty : i32
    {
        MinWidth = 0,
        PreferredWidth = 1,
        FlexibleWidth = 2,
        MinHeight = 3,
        PreferredHeight = 4,
        FlexibleHeight = 5,
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Root)
    class LayoutRebuilder : public OwnedRoot<LayoutRebuilder>, public ICanvasElement
    {
    public:
        explicit LayoutRebuilder(engine::RectTransform* root);
        ~LayoutRebuilder() override = default;

        HX_PROPERTY(ReadOnly) engine::Transform* transform() const override;
        HX_FUNCTION() bool IsDestroyed() const override;
        HX_FUNCTION() static void ForceRebuildLayoutImmediate(engine::RectTransform* layoutRoot);
        HX_FUNCTION() void Rebuild(CanvasUpdate executing) override;
        HX_FUNCTION() static void MarkLayoutForRebuild(engine::RectTransform* rect);
        HX_FUNCTION() void LayoutComplete() override;
        HX_FUNCTION() void GraphicUpdateComplete() override {}
        // ToString's text ("(Layout Rebuilder for) " and the root's name); one rebuilder per root, so equal rebuilders
        // are the same object.
        HX_FUNCTION(Internal) String Describe() const;

        scripting::TypeId HelixType() const override { return scripting::TypeOf<LayoutRebuilder>(); }

    private:
        engine::PPtr<engine::RectTransform> root_;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static)
    class LayoutUtility
    {
    public:
        HX_FUNCTION() static f32 GetMinSize(engine::RectTransform* rect, i32 axis);
        HX_FUNCTION() static f32 GetPreferredSize(engine::RectTransform* rect, i32 axis);
        HX_FUNCTION() static f32 GetFlexibleSize(engine::RectTransform* rect, i32 axis);
        HX_FUNCTION() static f32 GetMinWidth(engine::RectTransform* rect);
        HX_FUNCTION() static f32 GetPreferredWidth(engine::RectTransform* rect);
        HX_FUNCTION() static f32 GetFlexibleWidth(engine::RectTransform* rect);
        HX_FUNCTION() static f32 GetMinHeight(engine::RectTransform* rect);
        HX_FUNCTION() static f32 GetPreferredHeight(engine::RectTransform* rect);
        HX_FUNCTION() static f32 GetFlexibleHeight(engine::RectTransform* rect);
        // The property of the elements of rect: the highest priority's (the largest among equal priorities), negative
        // values ignored; the component that gave it through source.
        HX_FUNCTION(Internal, Out = "source") static f32 GetLayoutPropertyInternal(engine::RectTransform* rect, LayoutProperty property, f32 defaultValue,
                                                                                  engine::Component*& source);
    };

    // The layout's calls on any component (of C++ or of C#).
    namespace layout
    {
        bool IsLayoutElement(engine::Component* c);
        bool IsController(engine::Component* c, i32* kind = nullptr);
        bool IsGroup(engine::Component* c);
        bool IgnoresLayout(engine::Component* c);
        f32 Property(engine::Component* c, LayoutProperty property);
        i32 Priority(engine::Component* c);
        // 0 CalculateLayoutInputHorizontal, 1 CalculateLayoutInputVertical, 2 SetLayoutHorizontal, 3 SetLayoutVertical.
        void Call(engine::Component* c, i32 call);
        bool SkipsLayoutOf(engine::Component* c, const engine::RectTransform* rect);
    }
}

HX_CANVASUI_ROOT_HANDLES(helix::canvasui::LayoutRebuilder)
