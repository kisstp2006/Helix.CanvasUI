#include <Helix/CanvasUI/Layout/LayoutRebuilder.h>

#include <Helix/CanvasUI/Core/ManagedCalls.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Transform/RectTransform.h>

#include <limits>

namespace helix::canvasui
{
    // ------------------------------------------------------------------------------------------------- the calls

    namespace layout
    {
        namespace
        {
            UIBehaviour* Native(engine::Component* c) { return AsUIBehaviour(c); }

            bool Enabled(engine::Component* c) { return !c->Is<engine::Behaviour>() || static_cast<engine::Behaviour*>(c)->isActiveAndEnabled(); }
        }

        bool IsLayoutElement(engine::Component* c)
        {
            if (UIBehaviour* b = Native(c); b && b->AsLayoutElement())
                return true;
            return ManagedCalls::IsManaged(c) && ManagedCalls::Implements(c, "ILayoutElement");
        }

        bool IsController(engine::Component* c, i32* kind)
        {
            if (UIBehaviour* b = Native(c))
                if (ILayoutController* l = b->AsLayoutController())
                {
                    if (kind)
                        *kind = l->ControllerKind();
                    return true;
                }
            if (!ManagedCalls::IsManaged(c) || !ManagedCalls::Implements(c, "ILayoutController"))
                return false;
            if (kind)
                *kind = ManagedCalls::Implements(c, "ILayoutSelfController") ? 1 : ManagedCalls::Implements(c, "ILayoutGroup") ? 2 : 0;
            return true;
        }

        bool IsGroup(engine::Component* c)
        {
            i32 kind = 0;
            return IsController(c, &kind) && kind == 2;
        }

        bool IgnoresLayout(engine::Component* c)
        {
            if (UIBehaviour* b = Native(c))
                if (ILayoutIgnorer* i = b->AsLayoutIgnorer())
                    return i->ignoreLayout();
            return ManagedCalls::IsManaged(c) && ManagedCalls::Implements(c, "ILayoutIgnorer") && ManagedCalls::LayoutBool(c, 17);
        }

        f32 Property(engine::Component* c, LayoutProperty property)
        {
            if (UIBehaviour* b = Native(c))
                if (ILayoutElement* e = b->AsLayoutElement())
                    switch (property)
                    {
                    case LayoutProperty::MinWidth: return e->minWidth();
                    case LayoutProperty::PreferredWidth: return e->preferredWidth();
                    case LayoutProperty::FlexibleWidth: return e->flexibleWidth();
                    case LayoutProperty::MinHeight: return e->minHeight();
                    case LayoutProperty::PreferredHeight: return e->preferredHeight();
                    case LayoutProperty::FlexibleHeight: return e->flexibleHeight();
                    }
            return ManagedCalls::LayoutFloat(c, 10 + i32(property));
        }

        i32 Priority(engine::Component* c)
        {
            if (UIBehaviour* b = Native(c))
                if (ILayoutElement* e = b->AsLayoutElement())
                    return e->layoutPriority();
            return i32(ManagedCalls::LayoutFloat(c, 16));
        }

        void Call(engine::Component* c, i32 call)
        {
            if (UIBehaviour* b = Native(c))
            {
                if (call < 2)
                    if (ILayoutElement* e = b->AsLayoutElement())
                    {
                        call == 0 ? e->CalculateLayoutInputHorizontal() : e->CalculateLayoutInputVertical();
                        return;
                    }
                if (call >= 2)
                    if (ILayoutController* l = b->AsLayoutController())
                    {
                        call == 2 ? l->SetLayoutHorizontal() : l->SetLayoutVertical();
                        return;
                    }
            }
            if (ManagedCalls::IsManaged(c))
                ManagedCalls::LayoutCall(c, call);
        }

        bool SkipsLayoutOf(engine::Component* c, const engine::RectTransform* rect)
        {
            if (UIBehaviour* b = Native(c))
                if (ILayoutController* l = b->AsLayoutController())
                    return l->SkipsLayoutOf(rect);
            return false;
        }

        namespace
        {
            void ActiveComponents(engine::RectTransform* rect, bool (*keep)(engine::Component*), Array<engine::Component*>& out)
            {
                out.Clear();
                engine::GameObject* g = rect ? rect->gameObject() : nullptr;
                for (u32 i = 0; g && i < g->GetComponentCount(); i++)
                    if (engine::Component* c = g->GetComponentAtIndex(i); c && keep(c) && Enabled(c))
                        out.Push(c);
            }

            engine::RectTransform* AsRect(engine::Transform* t) { return t && t->Is<engine::RectTransform>() ? static_cast<engine::RectTransform*>(t) : nullptr; }

            bool ControllerOnly(engine::Component* c) { return IsController(c); }
            bool GroupOnly(engine::Component* c) { return IsGroup(c); }
            bool ElementOnly(engine::Component* c) { return IsLayoutElement(c); }

            // Top down: self controllers, then the others (groups), then the children (their own roots stop it).
            void Control(engine::RectTransform* rect, i32 call)
            {
                if (!rect)
                    return;
                Array<engine::Component*> controllers;
                ActiveComponents(rect, &ControllerOnly, controllers);
                if (controllers.Empty())
                    return;
                for (engine::Component* c : controllers)
                {
                    i32 kind = 0;
                    if (IsController(c, &kind) && kind == 1)
                        Call(c, call);
                }
                for (engine::Component* c : controllers)
                {
                    i32 kind = 0;
                    if (IsController(c, &kind) && kind != 1 && !SkipsLayoutOf(c, rect))
                        Call(c, call);
                }
                for (u32 i = 0; i < rect->childCount(); i++)
                    Control(AsRect(rect->GetChild(i)), call);
            }

            // Bottom up: the children, then the elements.
            void Calculate(engine::RectTransform* rect, i32 call)
            {
                if (!rect)
                    return;
                Array<engine::Component*> elements, groups;
                ActiveComponents(rect, &ElementOnly, elements);
                ActiveComponents(rect, &GroupOnly, groups);
                if (elements.Empty() && groups.Empty())
                {
                    // A disabled group still counts (the reference asks for any ILayoutGroup component).
                    engine::GameObject* g = rect->gameObject();
                    bool anyGroup = false;
                    for (u32 i = 0; g && i < g->GetComponentCount() && !anyGroup; i++)
                        anyGroup = IsGroup(g->GetComponentAtIndex(i));
                    if (!anyGroup)
                        return;
                }
                for (u32 i = 0; i < rect->childCount(); i++)
                    Calculate(AsRect(rect->GetChild(i)), call);
                for (engine::Component* c : elements)
                    Call(c, call);
            }

            bool ValidController(engine::RectTransform* root)
            {
                Array<engine::Component*> controllers;
                ActiveComponents(root, &ControllerOnly, controllers);
                return !controllers.Empty();
            }

            struct Rebuilders
            {
                // The rebuilders queued, one per root.
                Array<LayoutRebuilder*> queued;
                bool hooked = false;
            };

            Rebuilders& RB()
            {
                static Rebuilders* r = new Rebuilders();
                return *r;
            }

            void MarkRoot(engine::RectTransform* root)
            {
                if (!root)
                    return;
                Rebuilders& r = RB();
                for (LayoutRebuilder* q : r.queued)
                    if (q->transform() == root)
                        return;
                auto* rebuilder = new LayoutRebuilder(root);
                if (CanvasUpdateRegistry::TryRegisterCanvasElementForLayoutRebuildNative(rebuilder))
                    r.queued.Push(rebuilder);
                else
                    delete rebuilder;
            }
        }
    }

    // ------------------------------------------------------------------------------------------- LayoutRebuilder

    LayoutRebuilder::LayoutRebuilder(engine::RectTransform* root) : root_(root) {}

    engine::Transform* LayoutRebuilder::transform() const { return root_.Get(); }

    bool LayoutRebuilder::IsDestroyed() const { return root_.Get() == nullptr; }

    void LayoutRebuilder::ForceRebuildLayoutImmediate(engine::RectTransform* layoutRoot)
    {
        LayoutRebuilder rebuilder(layoutRoot);
        rebuilder.Rebuild(CanvasUpdate::Layout);
    }

    void LayoutRebuilder::Rebuild(CanvasUpdate executing)
    {
        if (executing != CanvasUpdate::Layout)
            return;
        engine::RectTransform* root = root_.Get();
        layout::Calculate(root, 0);
        layout::Control(root, 2);
        layout::Calculate(root, 1);
        layout::Control(root, 3);
    }

    void LayoutRebuilder::MarkLayoutForRebuild(engine::RectTransform* rect)
    {
        if (!rect || !rect->gameObject())
            return;
        layout::Rebuilders& r = layout::RB();
        if (!r.hooked)
        {
            r.hooked = true;
            // A driven RectTransform reverted: its layout sets it again.
            engine::RectTransform::reapplyDrivenPropertiesInternal().Add([](void*, engine::RectTransform* driven) { LayoutRebuilder::MarkLayoutForRebuild(driven); });
        }
        engine::RectTransform* root = rect;
        bool validGroup = true;
        for (engine::Transform* p = rect->parent(); validGroup && p && p->gameObject(); p = p->parent())
        {
            validGroup = false;
            if (!p->Is<engine::RectTransform>())
                break;
            Array<engine::Component*> groups;
            layout::ActiveComponents(static_cast<engine::RectTransform*>(p), &layout::GroupOnly, groups);
            if (!groups.Empty())
            {
                validGroup = true;
                root = static_cast<engine::RectTransform*>(p);
            }
        }
        if (root == rect && !layout::ValidController(root))
            return;
        layout::MarkRoot(root);
    }

    void LayoutRebuilder::LayoutComplete()
    {
        layout::Rebuilders& r = layout::RB();
        for (usize i = 0; i < r.queued.Size(); i++)
            if (r.queued[i] == this)
            {
                r.queued.RemoveAt(i);
                delete this;
                return;
            }
    }

    String LayoutRebuilder::Describe() const
    {
        engine::RectTransform* root = root_.Get();
        return Format("(Layout Rebuilder for) {}", root ? root->Name() : StringView("null"));
    }

    // --------------------------------------------------------------------------------------------- LayoutUtility

    f32 LayoutUtility::GetLayoutPropertyInternal(engine::RectTransform* rect, LayoutProperty property, f32 defaultValue, engine::Component*& source)
    {
        source = nullptr;
        if (!rect)
            return 0;
        f32 best = defaultValue;
        i32 maxPriority = std::numeric_limits<i32>::min();
        Array<engine::Component*> elements;
        layout::ActiveComponents(rect, &layout::ElementOnly, elements);
        for (engine::Component* c : elements)
        {
            i32 priority = layout::Priority(c);
            if (priority < maxPriority)
                continue;
            f32 value = layout::Property(c, property);
            if (value < 0)
                continue;
            if (priority > maxPriority)
            {
                best = value;
                maxPriority = priority;
                source = c;
            }
            else if (value > best)
            {
                best = value;
                source = c;
            }
        }
        return best;
    }

    namespace
    {
        f32 Get(engine::RectTransform* rect, LayoutProperty p)
        {
            engine::Component* source = nullptr;
            return LayoutUtility::GetLayoutPropertyInternal(rect, p, 0, source);
        }
    }

    f32 LayoutUtility::GetMinWidth(engine::RectTransform* rect) { return Get(rect, LayoutProperty::MinWidth); }
    f32 LayoutUtility::GetPreferredWidth(engine::RectTransform* rect)
    {
        f32 a = Get(rect, LayoutProperty::MinWidth), b = Get(rect, LayoutProperty::PreferredWidth);
        return a > b ? a : b;
    }
    f32 LayoutUtility::GetFlexibleWidth(engine::RectTransform* rect) { return Get(rect, LayoutProperty::FlexibleWidth); }
    f32 LayoutUtility::GetMinHeight(engine::RectTransform* rect) { return Get(rect, LayoutProperty::MinHeight); }
    f32 LayoutUtility::GetPreferredHeight(engine::RectTransform* rect)
    {
        f32 a = Get(rect, LayoutProperty::MinHeight), b = Get(rect, LayoutProperty::PreferredHeight);
        return a > b ? a : b;
    }
    f32 LayoutUtility::GetFlexibleHeight(engine::RectTransform* rect) { return Get(rect, LayoutProperty::FlexibleHeight); }
    f32 LayoutUtility::GetMinSize(engine::RectTransform* rect, i32 axis) { return axis == 0 ? GetMinWidth(rect) : GetMinHeight(rect); }
    f32 LayoutUtility::GetPreferredSize(engine::RectTransform* rect, i32 axis) { return axis == 0 ? GetPreferredWidth(rect) : GetPreferredHeight(rect); }
    f32 LayoutUtility::GetFlexibleSize(engine::RectTransform* rect, i32 axis) { return axis == 0 ? GetFlexibleWidth(rect) : GetFlexibleHeight(rect); }
}
