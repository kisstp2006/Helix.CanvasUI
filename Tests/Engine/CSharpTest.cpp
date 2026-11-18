#include <Support/CanvasTestKit.h>

#include <Helix/CanvasUI/Events/EventSystem.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::canvasui::testing;
using engine::GameObject;

HX_TEST(CSharp, GraphicsMeshEffectsLayoutAndCullingOfCSharp)
{
    HX_REQUIRE(StartScripts());
    ScreenSize(200, 100);
    Notes().Clear();
    GameObject* g = engine::GameObject::Create("G");
    g->AddComponent(CSharp("CsCanvasGraphics").type);
    engine::RunScriptPhase(engine::ScriptMessage::Update);
    HX_CHECK(NotesAre({"G:image:True:0:True:True", "G:quads:8:12:-20:-5:0:False", "G:cull:True:True", "G:layout:80:20:50", "G:vh:4:6"}));
    engine::testing::Cleanup();
}

HX_TEST(CSharp, TheEventSystemControlsAndRaycastersOfCSharp)
{
    HX_REQUIRE(StartScripts());
    ScreenSize(200, 100);
    Notes().Clear();
    GameObject* g = engine::GameObject::Create("E");
    g->AddComponent(CSharp("CsCanvasEvents").type);
    InputFrames in;
    in.Run();
    HX_CHECK(NotesAre({"E:raycast:Button:1:Strip:True", "E:execute:True:1:True:1:True:True", "E:structs:Explicit:True:1:0.1:3"}));
    in.Move(Vector2(1, 1)).Run();
    // Clicks on the button, the C# handler, the toggle, the slider (three quarters along), the trigger, the strip.
    for (Vector2 at : {Vector2(100, 50), Vector2(150, 50), Vector2(50, 50), Vector2(110, 20), Vector2(150, 20), Vector2(10, 50)})
    {
        in.Move(at).Run();
        in.Down(at).Run();
        in.Up(at).Run();
    }
    engine::SendScriptMessage(g, "Check");
    HX_CHECK(NotesAre({"E:heard:1:1:1:True:True:1:1"}));
    engine::testing::Cleanup();
}

HX_TEST(CSharp, DefaultControlsOfCSharpWithAFactoryOfCSharp)
{
    HX_REQUIRE(StartScripts());
    Notes().Clear();
    GameObject* g = engine::GameObject::Create("D");
    g->AddComponent(CSharp("CsDefaultControls").type);
    engine::RunScriptPhase(engine::ScriptMessage::Update);
    HX_CHECK(NotesAre({"D:defaults:Slider:Fill:9:Scroll View:Scrollbar Vertical:AutoHideAndExpandViewport"}));
    engine::testing::Cleanup();
}

HX_TEST(CSharp, TextInputValidationAndDropdownOptionsOfCSharp)
{
    HX_REQUIRE(StartScripts());
    ScreenSize(200, 100);
    Notes().Clear();
    {
        GameObject* es = engine::GameObject::Create("EventSystem");
        Add<canvasui::StandaloneInputModule>(es);
        GameObject* g = engine::GameObject::Create("T");
        g->AddComponent(CSharp("CsTextControls").type);
        InputFrames in;
        in.Run();
        in.Run();
        // onValidateInput upper-cases; the C# validator keeps digits.
        in.Type("ab1").Run();
        engine::SendScriptMessage(g, "FocusDigits");
        in.Run();
        in.Run();
        in.Type("a1b2").Run();
        engine::SendScriptMessage(g, "Check");
        HX_CHECK(NotesAre({"T:text:AB1:AB1:12:CustomValidator", "T:dropdown:3:2:2:Blue:Template"}));
        DestroyImmediate(es);
    }
    engine::testing::Cleanup();
}
