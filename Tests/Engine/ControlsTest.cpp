#include <Support/CanvasTestKit.h>

#include <Helix/CanvasUI/Controls/DefaultControls.h>
#include <Helix/CanvasUI/Controls/ScrollRect.h>
#include <Helix/CanvasUI/Controls/Sliders.h>
#include <Helix/CanvasUI/Events/EventSystem.h>
#include <Helix/CanvasUI/Masking/Masks.h>
#include <Helix/CanvasUI/Rendering/Image.h>
#include <Helix/Testing/Test.h>

#include <cmath>

using namespace helix;
using namespace helix::canvasui;
using namespace helix::canvasui::testing;
using engine::GameObject;

namespace
{
    struct Seen
    {
        Array<f32> values;
        Array<Vector2> positions;
    };
    void RememberValue(void* user, f32 v) { static_cast<Seen*>(user)->values.Push(v); }
    void RememberPosition(void* user, Vector2 v) { static_cast<Seen*>(user)->positions.Push(v); }

    // A RectTransform filling its parent (less the size delta).
    engine::GameObject* Stretched(StringView name, engine::Transform* parent, Vector2 sizeDelta = Vector2(0, 0))
    {
        GameObject* g = Element(name, parent, Vector2(0, 0), sizeDelta);
        Rt(g)->SetAnchorMin(Vector2(0, 0));
        Rt(g)->SetAnchorMax(Vector2(1, 1));
        return g;
    }

    struct Scene
    {
        EventSystem* es = nullptr;
        engine::Canvas* canvas = nullptr;

        Scene()
        {
            ScreenSize(200, 100);
            GameObject* g = engine::GameObject::Create("EventSystem");
            Add<StandaloneInputModule>(g);
            es = ComponentOf<EventSystem>(g);
            canvas = MakeCanvas("Canvas");
            Add<GraphicRaycaster>(canvas->gameObject());
        }
        ~Scene()
        {
            DestroyImmediate(canvas->gameObject());
            DestroyImmediate(es->gameObject());
            engine::testing::Cleanup();
        }
    };

    // A slider 100 x 20 around the screen's center (x 50 to 150): the track's image, a fill area and a handle area
    // filling it, the fill and a handle 10 wide.
    Slider* MakeSlider(engine::Transform* parent)
    {
        GameObject* g = Element("Slider", parent, Vector2(0, 0), Vector2(100, 20));
        Image* track = Add<Image>(g);
        GameObject* fill = Element("Fill", Stretched("Fill Area", g->transform())->transform(), Vector2(0, 0), Vector2(0, 0));
        Add<Image>(fill);
        GameObject* handle = Element("Handle", Stretched("Handle Slide Area", g->transform())->transform(), Vector2(0, 0), Vector2(10, 0));
        Add<Image>(handle);
        Slider* s = Add<Slider>(g);
        s->SetTargetGraphic(track);
        s->SetFillRect(Rt(fill));
        s->SetHandleRect(Rt(handle));
        return s;
    }
}

HX_TEST(Controls, SliderValuesFillsHandlesDragsAndKeys)
{
    HX_REQUIRE(RegisterModules());
    Scene scene;
    Slider* s = MakeSlider(scene.canvas->transform());
    Seen seen;
    s->onValueChanged().listeners().Add(&RememberValue, &seen);
    s->SetMinValue(0);
    s->SetMaxValue(10);
    s->SetValue(5);
    // The fill to the value, the handle at it.
    engine::RectTransform* fill = s->fillRect();
    engine::RectTransform* handle = s->handleRect();
    HX_CHECK(Near(s->normalizedValue(), 0.5f) && Near(fill->anchorMin(), Vector2(0, 0)) && Near(fill->anchorMax(), Vector2(0.5f, 1)));
    HX_CHECK(Near(handle->anchorMin(), Vector2(0.5f, 0)) && Near(handle->anchorMax(), Vector2(0.5f, 1)));
    HX_CHECK(seen.values.Size() == 1 && Near(seen.values[0], 5));
    // Clamped to the range; without notify no event.
    s->SetValue(12);
    HX_CHECK(Near(s->value(), 10) && seen.values.Size() == 2);
    s->SetValueWithoutNotify(5);
    HX_CHECK(Near(s->value(), 5) && seen.values.Size() == 2);
    InputFrames in;
    in.Move(Vector2(1, 1)).Run();
    // A press on the track beside the handle (three quarters along): the value jumps there.
    in.Move(Vector2(125, 50)).Run();
    in.Down(Vector2(125, 50)).Run();
    HX_CHECK(Near(s->value(), 7.5f));
    in.Up(Vector2(125, 50)).Run();
    // A drag of the handle (pressed at its center): the value follows the pointer, with no drag threshold.
    in.Down(Vector2(125, 50)).Run();
    in.Move(Vector2(60, 50)).Run();
    HX_CHECK(Near(s->value(), 1));
    in.Up(Vector2(60, 50)).Run();
    // Whole numbers: rounded.
    s->SetWholeNumbers(true);
    s->SetValue(3.4f);
    HX_CHECK(s->value() == 3);
    // Navigation along its axis moves it a step (one, for whole numbers); across, nothing to select.
    scene.es->SetSelectedGameObject(s->gameObject());
    in.Press(platform::KeyCode::RightArrow).Run();
    in.Release(platform::KeyCode::RightArrow).Run();
    HX_CHECK(s->value() == 4);
    in.Press(platform::KeyCode::LeftArrow).Run();
    in.Release(platform::KeyCode::LeftArrow).Run();
    HX_CHECK(s->value() == 3 && s->FindSelectableOnLeft() == nullptr);
    // Right to left: the fill from the right.
    s->SetDirection(Slider::Direction::RightToLeft, false);
    HX_CHECK(Near(fill->anchorMin(), Vector2(0.7f, 0)) && Near(fill->anchorMax(), Vector2(1, 1)) && Near(handle->anchorMin().x, 0.7f));
    // A filled image fills by its amount instead of its anchors.
    Image* fillImage = ComponentOf<Image>(fill->gameObject());
    fillImage->SetType(Image::Type::Filled);
    s->SetValue(6);
    HX_CHECK(Near(fillImage->fillAmount(), 0.6f));
    // Bottom to top, its layout flipped with it: the axis is vertical.
    s->SetDirection(Slider::Direction::BottomToTop, true);
    HX_CHECK(Near(Rt(s->gameObject())->sizeDelta(), Vector2(20, 100)));
    // (The flip moved the driven handle as a child; the next value places it on the new axis.)
    s->SetValue(7);
    HX_CHECK(Near(handle->anchorMin(), Vector2(0, 0.7f)) && Near(handle->anchorMax(), Vector2(1, 0.7f)));
}

HX_TEST(Controls, ScrollbarSizesStepsPagesAndDrags)
{
    HX_REQUIRE(RegisterModules());
    Scene scene;
    // A bar 100 x 20 around the center (x 50 to 150), its handle in a sliding area filling it.
    GameObject* g = Element("Scrollbar", scene.canvas->transform(), Vector2(0, 0), Vector2(100, 20));
    Image* track = Add<Image>(g);
    GameObject* handle = Stretched("Handle", Stretched("Sliding Area", g->transform())->transform());
    Add<Image>(handle);
    Scrollbar* bar = Add<Scrollbar>(g);
    bar->SetTargetGraphic(track);
    bar->SetHandleRect(Rt(handle));
    bar->SetSize(0.25f);
    Seen seen;
    bar->onValueChanged().listeners().Add(&RememberValue, &seen);
    HX_CHECK(Near(Rt(handle)->anchorMin(), Vector2(0, 0)) && Near(Rt(handle)->anchorMax(), Vector2(0.25f, 1)));
    bar->SetValue(1);
    HX_CHECK(Near(Rt(handle)->anchorMin().x, 0.75f) && Near(Rt(handle)->anchorMax().x, 1) && seen.values.Size() == 1);
    // Steps: the value snapped to four steps (0, 1/3, 2/3, 1).
    bar->SetNumberOfSteps(4);
    bar->SetValue(0.3f);
    HX_CHECK(Near(bar->value(), 1.0f / 3));
    bar->SetNumberOfSteps(0);
    bar->SetValue(0);
    InputFrames in;
    in.Move(Vector2(1, 1)).Run();
    // Pressed on the track right of the handle: a page (its size) now and at every frame's end until the handle is
    // under the point.
    in.Move(Vector2(140, 50)).Run();
    in.Down(Vector2(140, 50)).Run();
    HX_CHECK(Near(bar->value(), 0.5f));
    in.Run();
    HX_CHECK(Near(bar->value(), 0.75f));
    in.Run();
    HX_CHECK(Near(bar->value(), 1));
    in.Run();
    HX_CHECK(Near(bar->value(), 1));
    in.Up(Vector2(140, 50)).Run();
    // A drag of the handle (at its center, 137.5) to 62.5: the value to the start.
    in.Move(Vector2(137.5f, 50)).Down(Vector2(137.5f, 50)).Run();
    in.Move(Vector2(62.5f, 50)).Run();
    HX_CHECK(Near(bar->value(), 0, 1e-2f));
    in.Up(Vector2(62.5f, 50)).Run();
    // Keys move it a tenth (no steps).
    scene.es->SetSelectedGameObject(bar->gameObject());
    in.Press(platform::KeyCode::RightArrow).Run();
    in.Release(platform::KeyCode::RightArrow).Run();
    HX_CHECK(Near(bar->value(), 0.1f, 1e-2f));
    // Top to bottom: the handle from the top.
    bar->SetDirection(Scrollbar::Direction::TopToBottom, false);
    bar->SetValue(0);
    HX_CHECK(Near(Rt(handle)->anchorMin(), Vector2(0, 0.75f)) && Near(Rt(handle)->anchorMax(), Vector2(1, 1)));
}

HX_TEST(Controls, ScrollRectMovesItsContentByDragsWheelsAndScrollbars)
{
    HX_REQUIRE(RegisterModules());
    Scene scene;
    // A view 100 x 50 around the center (y 25 to 75) and content 100 x 200 in it (centered: half way).
    GameObject* g = Element("Scroll View", scene.canvas->transform(), Vector2(0, 0), Vector2(100, 50));
    Add<Image>(g);
    GameObject* viewport = Stretched("Viewport", g->transform());
    GameObject* content = Element("Content", viewport->transform(), Vector2(0, 0), Vector2(100, 200));
    Add<Image>(content);
    ScrollRect* scroll = Add<ScrollRect>(g);
    scroll->SetViewport(Rt(viewport));
    scroll->SetContent(Rt(content));
    scroll->SetHorizontal(false);
    scroll->SetInertia(false);
    // A vertical scrollbar beside it.
    GameObject* bg = Element("Scrollbar Vertical", scene.canvas->transform(), Vector2(70, 0), Vector2(10, 50));
    Add<Image>(bg);
    GameObject* handle = Stretched("Handle", Stretched("Sliding Area", bg->transform())->transform());
    Add<Image>(handle);
    Scrollbar* bar = Add<Scrollbar>(bg);
    bar->SetHandleRect(Rt(handle));
    bar->SetDirection(Scrollbar::Direction::BottomToTop, false);
    scroll->SetVerticalScrollbar(bar);
    Seen seen;
    scroll->onValueChanged().listeners().Add(&RememberPosition, &seen);
    Frame();
    HX_CHECK(Near(scroll->verticalNormalizedPosition(), 0.5f));
    // The bar: the view's share of the content, at the position.
    HX_CHECK(Near(bar->size(), 0.25f) && Near(bar->value(), 0.5f));
    // At the top: the content's top at the view's.
    scroll->SetVerticalNormalizedPosition(1);
    HX_CHECK(Near(Rt(content)->anchoredPosition().y, -75) && Near(scroll->verticalNormalizedPosition(), 1));
    // The bar moves the content.
    bar->SetValue(0);
    HX_CHECK(Near(Rt(content)->anchoredPosition().y, 75));
    scroll->SetVerticalNormalizedPosition(0.5f);
    InputFrames in;
    in.Move(Vector2(1, 1)).Run();
    // Back where the last layout left it: no change to tell.
    HX_CHECK(seen.positions.Empty());
    // A drag up of 10 after the threshold: the content up by 10 (no inertia).
    in.Move(Vector2(100, 50)).Down(Vector2(100, 50)).Run();
    in.Move(Vector2(100, 62)).Run();
    in.Move(Vector2(100, 72)).Run();
    HX_CHECK(Near(Rt(content)->anchoredPosition().y, 10));
    in.Up(Vector2(100, 72)).Run();
    HX_CHECK(Near(Rt(content)->anchoredPosition().y, 10) && Near(bar->value(), (75.0f - 10) / 150));
    HX_CHECK(!seen.positions.Empty() && Near(seen.positions[seen.positions.Size() - 1].y, (75.0f - 10) / 150));
    // The wheel (up) scrolls the content down by the sensitivity.
    scroll->SetScrollSensitivity(10);
    in.Move(Vector2(100, 50)).Wheel(0, 1).Run();
    HX_CHECK(Near(Rt(content)->anchoredPosition().y, 0));
    // Elastic: pulled past its end, it springs back over frames.
    scroll->SetContentAnchoredPosition(Vector2(0, 120));
    for (int i = 0; i < 12; i++)
        in.Run();
    HX_CHECK(Near(Rt(content)->anchoredPosition().y, 75, 0.5f));
    // Clamped: back in one frame.
    scroll->SetMovementType(ScrollRect::MovementType::Clamped);
    Rt(content)->SetAnchoredPosition(Vector2(0, 120));
    in.Run();
    HX_CHECK(Near(Rt(content)->anchoredPosition().y, 75));
    // Inertia: a velocity decelerates (by the rate to the frame's time) and moves the content.
    scroll->SetMovementType(ScrollRect::MovementType::Unrestricted);
    scroll->SetInertia(true);
    Rt(content)->SetAnchoredPosition(Vector2(0, 0));
    in.Run();
    scroll->SetVelocity(Vector2(0, 100));
    in.Run();
    f32 v = 100 * std::pow(0.135f, 0.25f);
    HX_CHECK(Near(scroll->velocity().y, v, 0.05f) && Near(Rt(content)->anchoredPosition().y, v * 0.25f, 0.05f));
    // Auto hide: content that fits needs no bar.
    scroll->SetVerticalScrollbarVisibility(ScrollRect::ScrollbarVisibility::AutoHide);
    scroll->SetInertia(false);
    scroll->StopMovement();
    Rt(content)->SetSizeDelta(Vector2(100, 40));
    in.Run();
    HX_CHECK(!bar->gameObject()->activeSelf());
    Rt(content)->SetSizeDelta(Vector2(100, 200));
    in.Run();
    HX_CHECK(bar->gameObject()->activeSelf());
}

namespace
{
    // A factory that counts what it makes (and makes it as the default one does).
    struct CountingFactory : IFactoryControls
    {
        Array<String> names;
        engine::GameObject* CreateGameObject(StringView name, Span<const scripting::TypeId> components) override
        {
            names.Push(String(name));
            engine::GameObject* g = engine::GameObject::Create(name);
            for (scripting::TypeId t : components)
                g->AddComponent(t);
            return g;
        }
    };

    engine::GameObject* Child(engine::GameObject* g, u32 index) { return g->transform()->GetChild(index)->gameObject(); }
}

HX_TEST(Controls, DefaultControlsMakeEveryControlsObject)
{
    HX_REQUIRE(RegisterModules());
    Scene scene;
    DefaultControls::Resources resources;
    // A panel stretched over its parent, half transparent.
    GameObject* panel = DefaultControls::CreatePanel(resources);
    HX_CHECK(panel->Name() == StringView("Panel") && Near(Rt(panel)->anchorMax(), Vector2(1, 1)) && Near(Rt(panel)->sizeDelta(), Vector2(0, 0)));
    HX_CHECK(Near(ComponentOf<Image>(panel)->color().a, 0.392f) && ComponentOf<Image>(panel)->type() == Image::Type::Sliced);
    // A button 160 x 30; images 100 x 100.
    GameObject* button = DefaultControls::CreateButton(resources);
    HX_CHECK(ComponentOf<Button>(button) && ComponentOf<Image>(button) && Near(Rt(button)->sizeDelta(), Vector2(160, 30)));
    HX_CHECK(Near(Rt(DefaultControls::CreateImage(resources))->sizeDelta(), Vector2(100, 100)) && ComponentOf<RawImage>(DefaultControls::CreateRawImage(resources)));
    // A slider: background, fill area with its fill, handle area with its handle, wired to the slider.
    GameObject* slider = DefaultControls::CreateSlider(resources);
    Slider* s = ComponentOf<Slider>(slider);
    HX_REQUIRE(s && slider->transform()->childCount() == 3);
    HX_CHECK(Child(slider, 0)->Name() == StringView("Background") && Child(slider, 1)->Name() == StringView("Fill Area") &&
             Child(slider, 2)->Name() == StringView("Handle Slide Area"));
    HX_CHECK(s->fillRect() == Rt(Child(Child(slider, 1), 0)) && s->handleRect() == Rt(Child(Child(slider, 2), 0)) && s->targetGraphic() == ComponentOf<Image>(Child(Child(slider, 2), 0)));
    HX_CHECK(Near(Rt(Child(slider, 1))->sizeDelta(), Vector2(-20, 0)) && Near(Rt(Child(slider, 1))->anchoredPosition(), Vector2(-5, 0)));
    // A scrollbar with its sliding area and handle; a toggle on, its checkmark the graphic.
    GameObject* scrollbar = DefaultControls::CreateScrollbar(resources);
    HX_CHECK(ComponentOf<Scrollbar>(scrollbar)->handleRect() == Rt(Child(Child(scrollbar, 0), 0)) && Near(Rt(Child(scrollbar, 0))->sizeDelta(), Vector2(-20, -20)));
    GameObject* toggle = DefaultControls::CreateToggle(resources);
    Toggle* t = ComponentOf<Toggle>(toggle);
    HX_CHECK(t->isOn() && t->graphic() == ComponentOf<Image>(Child(Child(toggle, 0), 0)) && Near(Rt(Child(toggle, 0))->anchoredPosition(), Vector2(10, -10)));
    // A scroll view: a masked viewport with its content, both bars; under a canvas, laid out, the content (300 tall in
    // 200) needs the vertical bar only, and the viewport makes room for it (20 wide, -3 apart).
    GameObject* view = DefaultControls::CreateScrollView(resources);
    view->transform()->SetParent(scene.canvas->transform(), false);
    ScrollRect* scroll = ComponentOf<ScrollRect>(view);
    HX_REQUIRE(scroll && view->transform()->childCount() == 3);
    GameObject* viewport = Child(view, 0);
    HX_CHECK(ComponentOf<Mask>(viewport) && !ComponentOf<Mask>(viewport)->showMaskGraphic() && scroll->viewport() == Rt(viewport) && scroll->content() == Rt(Child(viewport, 0)));
    HX_CHECK(scroll->horizontalScrollbar() == ComponentOf<Scrollbar>(Child(view, 1)) && scroll->verticalScrollbar() == ComponentOf<Scrollbar>(Child(view, 2)));
    HX_CHECK(ComponentOf<Scrollbar>(Child(view, 2))->direction() == Scrollbar::Direction::BottomToTop && Near(Rt(Child(view, 2))->sizeDelta(), Vector2(20, 0)));
    InputFrames in;
    in.Run();
    in.Run();
    HX_CHECK(Child(view, 2)->activeSelf() && !Child(view, 1)->activeSelf());
    HX_CHECK(Near(Rt(viewport)->sizeDelta(), Vector2(-17, 0)));
    HX_CHECK(Near(scroll->verticalScrollbar()->size(), 200.0f / 300, 1e-2f) && Near(scroll->verticalNormalizedPosition(), 1));
    // Another factory makes every object.
    CountingFactory counting;
    DefaultControls::SetFactory(&counting);
    GameObject* made = DefaultControls::CreateSlider(resources);
    DefaultControls::SetFactory(nullptr);
    HX_CHECK(counting.names.Size() == 6 && counting.names[0] == StringView("Slider") && counting.names[5] == StringView("Handle"));
    for (GameObject* g : {panel, button, slider, scrollbar, toggle, made})
        DestroyImmediate(g);
}
