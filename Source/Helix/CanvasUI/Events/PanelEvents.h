#pragma once

#include <Helix/CanvasUI/Events/EventSystem.h>
#include <Helix/UI/Engine/PanelInput.h>
#include <Helix/UI/Engine/UIView.h>

// The UI panels' input through the event system (panelInputRedirection): a panel raycaster hits a panel where an
// element is under the pointer (among the canvases by the panel's sorting order, over them when equal), a panel event
// handler gives the panel the pointer's moves, presses, releases and wheel, and the keys while it is selected. The
// event system makes both for every panel on GameObjects of its own while the input is redirected
// (autoCreatePanelComponents).
namespace helix::canvasui
{
    HX_CLASS(Namespace = "HelixEngine.UIEvents")
    class PanelRaycaster : public BaseRaycaster
    {
    public:
        HX_PROPERTY(Internal, Name = "panelId") u64 panelId() const { return panel_; }
        void SetPanelId(u64 value) { panel_ = value; }
        // The panel's first view (its panel in C#).
        HX_PROPERTY(ReadOnly, Internal) ui::UIView* panelViewInternal() const;
        // For C#: the panels by id and their first views.
        HX_FUNCTION(Internal) static Array<u64> PanelIdsInternal() { return ui::PanelInput::Panels(); }
        HX_FUNCTION(Internal) static ui::UIView* PanelViewOfInternal(u64 panel) { return ui::PanelInput::FirstView(panel); }
        HX_PROPERTY(ReadOnly) engine::Camera* eventCamera() const override { return nullptr; }
        HX_PROPERTY(ReadOnly) i32 sortOrderPriority() const override;
        HX_PROPERTY(ReadOnly) i32 renderOrderPriority() const override;
        void RaycastInternal(PointerEventData* eventData) override;

    private:
        u64 panel_ = 0;
    };

    HX_CLASS(Namespace = "HelixEngine.UIEvents")
    class PanelEventHandler : public UIBehaviour,
                              public INativeEventHandler,
                              public IPointerMoveHandler,
                              public IPointerUpHandler,
                              public IPointerDownHandler,
                              public ISubmitHandler,
                              public ICancelHandler,
                              public IMoveHandler,
                              public IScrollHandler,
                              public ISelectHandler,
                              public IDeselectHandler,
                              public IPointerExitHandler,
                              public IPointerEnterHandler,
                              public IPointerClickHandler
    {
    public:
        HX_PROPERTY(Internal, Name = "panelId") u64 panelId() const { return panel_; }
        void SetPanelId(u64 value) { panel_ = value; }
        HX_PROPERTY(ReadOnly, Internal) ui::UIView* panelViewInternal() const;

        HX_FUNCTION() virtual void OnPointerMove(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnPointerUp(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnPointerDown(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnPointerExit(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnPointerEnter(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnPointerClick(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnSubmit(BaseEventData* eventData) override;
        HX_FUNCTION() virtual void OnCancel(BaseEventData* eventData) override;
        HX_FUNCTION() virtual void OnMove(AxisEventData* eventData) override;
        HX_FUNCTION() virtual void OnScroll(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnSelect(BaseEventData* eventData) override;
        HX_FUNCTION() virtual void OnDeselect(BaseEventData* eventData) override;
        HX_FUNCTION(Protected) virtual void Update();

        bool Handles(EventKind kind) const override;
        void HandleEvent(EventKind kind, BaseEventData* data) override;
        INativeEventHandler* AsEventHandler() override { return this; }
        void OnDisable() override;

        // The event system's components of the panels: made for the panels that take input while it redirects
        // their input, removed with their panels (and all of them when the event system goes).
        static void UpdatePanelComponents(EventSystem* system);
        static void RemovePanelComponents();

    private:
        // The pointer on the panel: the panel's point of the event's position; false when it is not on the panel.
        bool PanelPoint(PointerEventData* eventData, Vector2& at) const;
        void SendMove(PointerEventData* eventData, bool covered);

        u64 panel_ = 0;
        u64 keysFrame_ = ~u64(0);
        // Pressed on the panel: its moves go on outside it until the release (the panel's capture).
        bool pressed_ = false;
        Vector2 lastScreen_ = Vector2(0, 0);
    };
}
