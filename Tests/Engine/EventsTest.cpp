#include <Support/CanvasTestKit.h>

#include <Helix/CanvasUI/Controls/Selectable.h>
#include <Helix/CanvasUI/Events/EventSystem.h>
#include <Helix/CanvasUI/Rendering/Image.h>
#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Testing/Test.h>

#include <Helix/Engine/Transform/RectTransformUtility.h>
#include <cstdio>
#include <initializer_list>

using namespace helix;
using namespace helix::canvasui;
using namespace helix::canvasui::testing;
using engine::GameObject;

namespace
{
    void Count(void* user) { ++*static_cast<i32*>(user); }
    void CountData(void* user, BaseEventData*) { ++*static_cast<i32*>(user); }

    struct Values
    {
        Array<bool> seen;
    };
    void Remember(void* user, bool value) { static_cast<Values*>(user)->seen.Push(value); }

    // An event system with the standalone module on its own GameObject (the module requires the system).
    EventSystem* MakeEventSystem()
    {
        GameObject* g = engine::GameObject::Create("EventSystem");
        Add<StandaloneInputModule>(g);
        EventSystem* es = ComponentOf<EventSystem>(g);
        return es;
    }

    // A button: its image the target graphic, its click counted.
    Button* MakeButton(StringView name, engine::Transform* parent, Vector2 position, Vector2 size, i32* clicks)
    {
        GameObject* g = Element(name, parent, position, size);
        Image* image = Add<Image>(g);
        Button* b = Add<Button>(g);
        b->SetTargetGraphic(image);
        if (clicks)
            b->onClick().listeners().Add(&Count, clicks);
        return b;
    }

    // A trigger entry of the type counted on the GameObject's trigger (added when missing).
    void CountEntry(GameObject* g, EventTriggerType type, i32* count)
    {
        EventTrigger* t = ComponentOf<EventTrigger>(g);
        if (!t)
            t = Add<EventTrigger>(g);
        t->AddEntry(type);
        t->EntryStorage(t->EntryCount() - 1);
        static_cast<engine::HelixEvent<BaseEventData*>*>(t->EntryStorage(t->EntryCount() - 1))->listeners().Add(&CountData, count);
    }
}

HX_TEST(Events, TheMouseHighlightsPressesAndClicksAButton)
{
    HX_REQUIRE(RegisterModules());
    engine::InputFeed::Reset();
    ScreenSize(200, 100);
    EventSystem* es = MakeEventSystem();
    HX_REQUIRE(es && EventSystem::current() == es);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    Add<GraphicRaycaster>(canvas->gameObject());
    i32 clicks = 0;
    // The screen's center is the canvas's: the button 40 x 20 around (100, 50).
    Button* button = MakeButton("Button", canvas->transform(), Vector2(0, 0), Vector2(40, 20), &clicks);
    InputFrames in;
    in.Run();
    // A raycast: the image under the point, nothing beside it.
    PointerEventData* data = new PointerEventData(es);
    data->SetPosition(Vector2(100, 50));
    Array<RaycastResult> hits;
    es->RaycastAll(data, hits);
    HX_CHECK(hits.Size() == 1 && hits[0].gameObject.Get() == button->gameObject() && hits[0].module.Get() == ComponentOf<GraphicRaycaster>(canvas->gameObject()));
    data->SetPosition(Vector2(10, 10));
    es->RaycastAll(data, hits);
    HX_CHECK(hits.Empty());
    delete data;
    // Over it: highlighted (its color tint the highlighted color).
    in.Move(Vector2(100, 50)).Run();
    HX_CHECK(es->IsPointerOverGameObject());
    HX_CHECK(button->currentSelectionState() == Selectable::SelectionState::Highlighted);
    // Pressed, then released over it: one click, and it is selected.
    in.Down(Vector2(100, 50)).Run();
    HX_CHECK(button->currentSelectionState() == Selectable::SelectionState::Pressed && clicks == 0);
    HX_CHECK(es->currentSelectedGameObject() == button->gameObject());
    in.Up(Vector2(100, 50)).Run();
    HX_CHECK(clicks == 1);
    // Pressed over it, released beside it: no click.
    in.Down(Vector2(100, 50)).Run();
    in.Move(Vector2(10, 10)).Up(Vector2(10, 10)).Run();
    HX_CHECK(clicks == 1 && !es->IsPointerOverGameObject());
    // Not interactable: no click, disabled.
    button->SetInteractable(false);
    in.Move(Vector2(100, 50)).Down(Vector2(100, 50)).Run();
    in.Up(Vector2(100, 50)).Run();
    HX_CHECK(clicks == 1 && button->currentSelectionState() == Selectable::SelectionState::Disabled);
    DestroyImmediate(canvas->gameObject());
    DestroyImmediate(es->gameObject());
    engine::testing::Cleanup();
}

HX_TEST(Events, KeysNavigateBetweenSelectablesAndSubmit)
{
    HX_REQUIRE(RegisterModules());
    engine::InputFeed::Reset();
    ScreenSize(200, 100);
    EventSystem* es = MakeEventSystem();
    engine::Canvas* canvas = MakeCanvas("Canvas");
    Add<GraphicRaycaster>(canvas->gameObject());
    i32 left = 0, right = 0;
    Button* a = MakeButton("Left", canvas->transform(), Vector2(-50, 0), Vector2(40, 20), &left);
    Button* b = MakeButton("Right", canvas->transform(), Vector2(50, 0), Vector2(40, 20), &right);
    Button* below = MakeButton("Below", canvas->transform(), Vector2(-50, -35), Vector2(40, 20), nullptr);
    HX_CHECK(Selectable::allSelectableCount() == 3);
    // Automatic navigation: the nearest that way.
    HX_CHECK(a->FindSelectableOnRight() == b && b->FindSelectableOnLeft() == a && a->FindSelectableOnDown() == below && a->FindSelectableOnUp() == nullptr);
    es->SetSelectedGameObject(a->gameObject());
    InputFrames in;
    in.Move(Vector2(1, 1)).Run();
    HX_CHECK(a->currentSelectionState() == Selectable::SelectionState::Selected);
    // Right: the selection moves; Return submits the selected one.
    in.Press(platform::KeyCode::RightArrow).Run();
    in.Release(platform::KeyCode::RightArrow).Run();
    HX_CHECK(es->currentSelectedGameObject() == b->gameObject() && a->currentSelectionState() == Selectable::SelectionState::Normal);
    in.Press(platform::KeyCode::Return).Run();
    in.Release(platform::KeyCode::Return).Run();
    HX_CHECK(right == 1 && left == 0);
    // Explicit navigation: only the ones it names.
    NavigationData nav;
    nav.mode = NavigationMode::Explicit;
    nav.selectOnRight = below->GetEntityId();
    b->SetNavigationOf(nav);
    HX_CHECK(b->FindSelectableOnRight() == below && b->FindSelectableOnLeft() == nullptr);
    // None: no navigation at all.
    nav.mode = NavigationMode::None;
    a->SetNavigationOf(nav);
    HX_CHECK(a->FindSelectableOnRight() == nullptr);
    // Without navigation events the keys do nothing.
    es->SetSendNavigationEvents(false);
    in.Press(platform::KeyCode::Return).Run();
    in.Release(platform::KeyCode::Return).Run();
    HX_CHECK(right == 1);
    DestroyImmediate(canvas->gameObject());
    DestroyImmediate(es->gameObject());
    engine::testing::Cleanup();
}

HX_TEST(Events, TogglesGroupsAndTriggers)
{
    HX_REQUIRE(RegisterModules());
    engine::InputFeed::Reset();
    ScreenSize(200, 100);
    EventSystem* es = MakeEventSystem();
    engine::Canvas* canvas = MakeCanvas("Canvas");
    Add<GraphicRaycaster>(canvas->gameObject());
    // Two toggles of one group (no switching off): one is on, a click on the other turns it on and the first off.
    ToggleGroup* group = Add<ToggleGroup>(Element("Group", canvas->transform(), Vector2(0, 0), Vector2(200, 100)));
    Toggle* toggles[2];
    Values seen[2];
    for (int i = 0; i < 2; i++)
    {
        GameObject* g = Element(i == 0 ? "First" : "Second", group->transform(), Vector2(i == 0 ? -50.0f : 50.0f, 0), Vector2(40, 20));
        Image* image = Add<Image>(g);
        Image* check = Add<Image>(Element("Check", g->transform(), Vector2(0, 0), Vector2(10, 10)));
        toggles[i] = Add<Toggle>(g);
        toggles[i]->SetTargetGraphic(image);
        toggles[i]->SetGraphic(check);
        toggles[i]->onValueChanged().listeners().Add(&Remember, &seen[i]);
    }
    toggles[0]->SetIsOn(true);
    toggles[0]->SetGroup(group);
    toggles[1]->SetGroup(group);
    HX_CHECK(group->AnyTogglesOn() && group->GetFirstActiveToggle() == toggles[0] && seen[0].seen.Size() == 1);
    InputFrames in;
    in.Move(Vector2(1, 1)).Run();
    in.Move(Vector2(150, 50)).Down(Vector2(150, 50)).Run();
    in.Up(Vector2(150, 50)).Run();
    HX_CHECK(toggles[1]->isOn() && !toggles[0]->isOn());
    HX_CHECK(seen[1].seen.Size() == 1 && seen[1].seen[0] && seen[0].seen.Size() == 2 && !seen[0].seen[1]);
    // The check graphic fades with the value (instantly in edit: its canvas renderer's alpha).
    HX_CHECK(ComponentOf<Image>(toggles[1]->transform()->GetChild(0)->gameObject())->canvasRenderer()->GetAlpha() > 0.99f);
    // A click on the one that is on: it stays on (the group allows no switching off).
    in.Down(Vector2(150, 50)).Run();
    in.Up(Vector2(150, 50)).Run();
    HX_CHECK(toggles[1]->isOn());
    group->SetAllowSwitchOff(true);
    in.Down(Vector2(150, 50)).Run();
    in.Up(Vector2(150, 50)).Run();
    HX_CHECK(!toggles[1]->isOn() && !group->AnyTogglesOn());
    // Without notify: no event.
    usize before = seen[0].seen.Size();
    toggles[0]->SetIsOnWithoutNotify(true);
    HX_CHECK(toggles[0]->isOn() && seen[0].seen.Size() == before);

    // A trigger: its entries hear their events (enter, down, up, click; a drag's begin, drags and end).
    GameObject* pad = Element("Pad", canvas->transform(), Vector2(0, 30), Vector2(40, 20));
    Add<Image>(pad);
    i32 enter = 0, down = 0, up = 0, click = 0, begin = 0, drag = 0, end = 0, exit = 0;
    CountEntry(pad, EventTriggerType::PointerEnter, &enter);
    CountEntry(pad, EventTriggerType::PointerDown, &down);
    CountEntry(pad, EventTriggerType::PointerUp, &up);
    CountEntry(pad, EventTriggerType::PointerClick, &click);
    CountEntry(pad, EventTriggerType::BeginDrag, &begin);
    CountEntry(pad, EventTriggerType::Drag, &drag);
    CountEntry(pad, EventTriggerType::EndDrag, &end);
    CountEntry(pad, EventTriggerType::PointerExit, &exit);
    // Raycast once drawn (a graphic has its depth from the canvas's batching).
    Frame();
    in.Move(Vector2(100, 80)).Run();
    HX_CHECK(enter == 1);
    in.Down(Vector2(100, 80)).Run();
    in.Up(Vector2(100, 80)).Run();
    HX_CHECK(down == 1 && up == 1 && click == 1 && begin == 0);
    // A drag past the threshold (10 pixels): begin, drags, end on release.
    in.Down(Vector2(100, 80)).Run();
    in.Move(Vector2(103, 80)).Run();
    HX_CHECK(begin == 0);
    in.Move(Vector2(120, 80)).Run();
    in.Move(Vector2(130, 80)).Run();
    HX_CHECK(begin == 1 && drag == 2 && end == 0);
    in.Up(Vector2(130, 80)).Run();
    HX_CHECK(end == 1 && up == 2);
    in.Move(Vector2(5, 5)).Run();
    HX_CHECK(exit == 1);
    DestroyImmediate(canvas->gameObject());
    DestroyImmediate(es->gameObject());
    engine::testing::Cleanup();
}

HX_TEST(Events, RaycastsHitCameraAndWorldCanvasesIn3D)
{
    HX_REQUIRE(RegisterModules());
    engine::InputFeed::Reset();
    ScreenSize(200, 100);
    EventSystem* es = MakeEventSystem();
    // The camera looks along +z from z = -10.
    GameObject* eye = engine::GameObject::Create("Eye");
    eye->transform()->SetPosition(Vector3(0, 0, -10));
    engine::Camera* camera = eye->AddComponent<engine::Camera>();
    camera->SetFieldOfView(60);
    // A world canvas at the origin, 2 x 2 units (200 x 200 at 0.01), turned 50 degrees about y.
    engine::Canvas* world = MakeCanvas("World", engine::RenderMode::WorldSpace);
    world->SetWorldCamera(camera);
    Rt(world->gameObject())->SetSizeDelta(Vector2(200, 200));
    world->transform()->SetLocalScale(Vector3(0.01f, 0.01f, 0.01f));
    world->transform()->SetRotation(Quaternion::Euler(0, 50, 0));
    Add<GraphicRaycaster>(world->gameObject());
    i32 clicks = 0;
    Button* panel = MakeButton("Panel", world->transform(), Vector2(0, 0), Vector2(100, 100), &clicks);
    InputFrames in;
    in.Run();
    PointerEventData* data = new PointerEventData(es);
    data->SetPosition(Vector2(100, 50));
    Array<RaycastResult> hits;
    es->RaycastAll(data, hits);
    HX_REQUIRE(hits.Size() == 1);
    HX_CHECK(hits[0].gameObject.Get() == panel->gameObject() && Near(hits[0].distance, 9.7f, 1e-2f) && Near(hits[0].worldPosition, Vector3(0, 0, 0), 1e-2f));
    // (Distances are from the camera's near plane, 0.3 units in front of it.)
    // Beside the panel (its half width turned: 0.5 * cos 50 = 0.32 units), nothing; inside it, a hit further away or
    // nearer by the turn.
    engine::Camera* c = camera;
    Vector3 edge = c->WorldToScreenPoint(Vector3(0.6f, 0, 0));
    data->SetPosition(Vector2(edge.x, edge.y));
    es->RaycastAll(data, hits);
    HX_CHECK(hits.Empty());
    Vector3 inside = c->WorldToScreenPoint(Quaternion::Euler(0, 50, 0) * Vector3(0.3f, 0, 0));
    data->SetPosition(Vector2(inside.x, inside.y));
    es->RaycastAll(data, hits);
    HX_CHECK(hits.Size() == 1 && Near(hits[0].worldPosition, Quaternion::Euler(0, 50, 0) * Vector3(0.3f, 0, 0), 1e-2f));
    // A click on it in 3D.
    in.Move(Vector2(100, 50)).Down(Vector2(100, 50)).Run();
    in.Up(Vector2(100, 50)).Run();
    HX_CHECK(clicks == 1);
    // Turned away from the camera (reversed): ignored, or hit when reversed graphics count.
    world->transform()->SetRotation(Quaternion::Euler(0, 180, 0));
    data->SetPosition(Vector2(100, 50));
    es->RaycastAll(data, hits);
    HX_CHECK(hits.Empty());
    ComponentOf<GraphicRaycaster>(world->gameObject())->SetIgnoreReversedGraphics(false);
    es->RaycastAll(data, hits);
    HX_CHECK(hits.Size() == 1);
    // A camera canvas in front of it (5 units away) is hit first: its graphic before the world's in the sorted hits.
    engine::Canvas* view = MakeCanvas("View", engine::RenderMode::ScreenSpaceCamera);
    view->SetWorldCamera(camera);
    view->SetPlaneDistance(5);
    Add<GraphicRaycaster>(view->gameObject());
    Button* hud = MakeButton("Hud", view->transform(), Vector2(0, 0), Vector2(40, 20), nullptr);
    Frame();
    es->RaycastAll(data, hits);
    HX_REQUIRE(hits.Size() == 2);
    HX_CHECK(hits[0].gameObject.Get() == hud->gameObject() && Near(hits[0].distance, 4.7f, 1e-2f) && hits[1].gameObject.Get() == panel->gameObject());
    // An overlay canvas is over them all.
    engine::Canvas* overlay = MakeCanvas("Overlay");
    Add<GraphicRaycaster>(overlay->gameObject());
    Button* top = MakeButton("Top", overlay->transform(), Vector2(0, 0), Vector2(20, 20), nullptr);
    Frame();
    es->RaycastAll(data, hits);
    HX_CHECK(hits.Size() == 3 && hits[0].gameObject.Get() == top->gameObject());
    delete data;
    for (engine::Canvas* x : {world, view, overlay})
        DestroyImmediate(x->gameObject());
    DestroyImmediate(eye);
    DestroyImmediate(es->gameObject());
    engine::testing::Cleanup();
}
