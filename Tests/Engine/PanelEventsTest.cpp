#include <Support/CanvasTestKit.h>

#include <Helix/CanvasUI/Controls/Selectable.h>
#include <Helix/CanvasUI/Events/PanelEvents.h>
#include <Helix/CanvasUI/Rendering/Image.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Core/Types/Guid.h>
#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Testing/Test.h>
#include <Helix/UI/Engine/PanelInput.h>
#include <Helix/UI/Engine/PanelInputConfiguration.h>
#include <Helix/UI/Engine/UIAssets.h>
#include <Helix/UI/Events/EventSystem.h>

using namespace helix;
using namespace helix::canvasui;
using namespace helix::canvasui::testing;
using engine::GameObject;

namespace
{
    // A button of the panel 32 x 24 at (8, 8) from the top left (screen pixels: x 8 to 40, y 68 to 92 from the bottom).
    constexpr StringView kPage = R"(<HXML>
  <Style src="panel.hss"/>
  <Element name="button" focusable="true"/>
</HXML>)";

    constexpr StringView kSheet = R"(
#button { position: absolute; left: 8px; top: 8px; width: 32px; height: 24px; background-color: #ff0000; }
)";

    // The markup and its sheet in a folder of assets (gone with the test's objects).
    struct PageFolder
    {
        String root;
        engine::FolderAssetSource* source = nullptr;
        Array<String> files;

        PageFolder()
        {
            root = platform::JoinPath(platform::TempDirectory(), Format("helix-canvasui-{}", Guid::Generate().ToString()));
            platform::CreateDirectories(root);
            Write("panel.hxml", kPage);
            Write("panel.hss", kSheet);
            source = new engine::FolderAssetSource(root);
            engine::AddAssetSource(source);
        }

        void Write(StringView path, StringView text)
        {
            String file = platform::JoinPath(root, path);
            platform::WriteFile(file, Span<const u8>(reinterpret_cast<const u8*>(text.Data()), text.Size()));
            files.Push(file);
        }

        ~PageFolder()
        {
            engine::testing::Cleanup();
            while (engine::PendingDestroyCount())
                engine::ProcessDestroyQueue(), engine::SetLifecycleTime(engine::LifecycleTime() + 1e9);
            engine::SetLifecycleTime(0);
            engine::ResetAssets();
            delete source;
            for (const String& f : files)
                platform::RemoveFile(f);
            platform::RemoveEmptyDirectory(root);
        }
    };

    struct Counts
    {
        i32 clicks = 0, keys = 0;
    };
}

HX_TEST(PanelEvents, ThePanelsInputComesThroughTheEventSystem)
{
    HX_REQUIRE(RegisterModules());
    PageFolder folder;
    ScreenSize(200, 100);
    // A view of the page.
    GameObject* ui = engine::GameObject::Create("UI");
    auto* view = static_cast<ui::UIView*>(ui->AddComponent(scripting::TypeOf<ui::UIView>()));
    HX_REQUIRE(view);
    const engine::AssetEntry* page = engine::FindAssetAtPath("panel.hxml");
    HX_REQUIRE(page);
    view->SetSource(ui::ScriptObjectAs<ui::UITemplate>(engine::LoadAsset(page->guid)));
    InputFrames in;
    view->Update();
    ui::EventSystem& events = view->panel()->events();
    ui::ElementHandle button = view->panel()->tree().FirstChild(view->rootElement()->handle());
    HX_REQUIRE(button.IsValid());
    Counts counts;
    const ui::EventTypes& t = ui::Events();
    events.RegisterCallback(button, t.click, [](void* u, ui::EventSystem&, ui::UIEvent&) { static_cast<Counts*>(u)->clicks++; }, &counts);
    events.RegisterCallback(button, t.keyDown, [](void* u, ui::EventSystem&, ui::UIEvent&) { static_cast<Counts*>(u)->keys++; }, &counts);
    HX_CHECK(ui::PanelInput::ReadsOwnInput());

    // An event system: the panel reads no input of its own now, and the event system made its components.
    GameObject* esObject = engine::GameObject::Create("EventSystem");
    Add<StandaloneInputModule>(esObject);
    EventSystem* es = ComponentOf<EventSystem>(esObject);
    HX_REQUIRE(es);
    HX_CHECK(!ui::PanelInput::ReadsOwnInput());
    // A canvas button beside the panel's (x 120 to 160) and one under it.
    engine::Canvas* canvas = MakeCanvas("Canvas");
    Add<GraphicRaycaster>(canvas->gameObject());
    i32 canvasClicks = 0, underClicks = 0;
    auto button2 = [&](StringView name, Vector2 at, i32* clicks) {
        GameObject* g = Element(name, canvas->transform(), at, Vector2(40, 20));
        Image* image = Add<Image>(g);
        Button* b = Add<Button>(g);
        b->SetTargetGraphic(image);
        b->onClick().listeners().Add([](void* u) { ++*static_cast<i32*>(u); }, clicks);
    };
    button2("Beside", Vector2(40, 0), &canvasClicks);
    button2("Under", Vector2(-76, 30), &underClicks);
    in.Move(Vector2(1, 1)).Run();
    engine::Transform* systemTransform = esObject->transform();
    HX_REQUIRE(systemTransform->childCount() == 1);
    GameObject* panelObject = systemTransform->GetChild(0)->gameObject();
    PanelRaycaster* raycaster = ComponentOf<PanelRaycaster>(panelObject);
    PanelEventHandler* handler = ComponentOf<PanelEventHandler>(panelObject);
    HX_REQUIRE(raycaster && handler && raycaster->panelId() == handler->panelId() && ui::PanelInput::Alive(raycaster->panelId()));
    HX_CHECK(raycaster->panelViewInternal() == view);

    // A raycast over the panel's button: the panel first (over the canvas button under it), then the canvas.
    PointerEventData* data = new PointerEventData(es);
    data->SetPosition(Vector2(20, 80));
    Array<RaycastResult> hits;
    es->RaycastAll(data, hits);
    HX_REQUIRE(hits.Size() == 2);
    HX_CHECK(hits[0].gameObject.Get() == panelObject && hits[0].module.Get() == raycaster && hits[1].gameObject->Name() == StringView("Under"));
    // Beside the panel's elements: no panel hit.
    data->SetPosition(Vector2(100, 10));
    es->RaycastAll(data, hits);
    HX_CHECK(hits.Empty());
    delete data;

    // A click on the panel's button reaches the panel (and not the canvas under it); the panel is selected and
    // focuses the button.
    in.Move(Vector2(20, 80)).Run();
    in.Down(Vector2(20, 80)).Run();
    in.Up(Vector2(20, 80)).Run();
    HX_CHECK(counts.clicks == 1 && underClicks == 0 && es->currentSelectedGameObject() == panelObject && events.Focused() == button);
    // Keys while it is selected go to it.
    in.Press(platform::KeyCode::A).Run();
    in.Release(platform::KeyCode::A).Run();
    HX_CHECK(counts.keys == 1);
    // A canvas button: clicked as before; the panel deselected (its focus gone), and keys no longer reach it.
    in.Move(Vector2(140, 50)).Down(Vector2(140, 50)).Run();
    in.Up(Vector2(140, 50)).Run();
    HX_CHECK(canvasClicks == 1 && counts.clicks == 1 && es->currentSelectedGameObject() != panelObject && !events.Focused().IsValid());
    in.Press(platform::KeyCode::A).Run();
    in.Release(platform::KeyCode::A).Run();
    HX_CHECK(counts.keys == 1);

    // No redirection: the panel reads its own input again and the components go.
    GameObject* configObject = engine::GameObject::Create("Config");
    auto* config = static_cast<ui::PanelInputConfiguration*>(configObject->AddComponent(scripting::TypeOf<ui::PanelInputConfiguration>()));
    HX_REQUIRE(config);
    config->SetPanelInputRedirectionInternal(1);
    in.Run();
    HX_CHECK(ui::PanelInput::ReadsOwnInput() && systemTransform->childCount() == 0);
    in.Move(Vector2(20, 80)).Down(Vector2(20, 80)).Run();
    in.Up(Vector2(20, 80)).Run();
    view->Update();
    HX_CHECK(counts.clicks == 2);
    // Always: through the event system again; without one, the panel gets no input at all.
    config->SetPanelInputRedirectionInternal(2);
    in.Run();
    HX_CHECK(!ui::PanelInput::ReadsOwnInput() && systemTransform->childCount() == 1);
    DestroyImmediate(esObject);
    HX_CHECK(!ui::PanelInput::ReadsOwnInput());
    in.Move(Vector2(20, 80)).Down(Vector2(20, 80)).Run();
    in.Up(Vector2(20, 80)).Run();
    view->Update();
    HX_CHECK(counts.clicks == 2);
}
