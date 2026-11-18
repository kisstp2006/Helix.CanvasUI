#include <Support/Devices.h>
#include <Support/Golden.h>
#include <Support/TextKit.h>

#include <Helix/CanvasUI/Text/CanvasText.h>
#include <Helix/CanvasUI/Text/WorldText.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Testing/Test.h>

#include <cstdio>

using namespace helix;
using namespace helix::canvasui;
using namespace helix::canvasui::testing;
using engine::GameObject;

// Text on an overlay canvas (plain, outlined, with an underlay) and a world text before the camera, on every device.
HX_TEST(TextRender, CanvasAndWorldTextDrawOnEveryDevice)
{
    HX_REQUIRE(RegisterModules());
    constexpr u32 width = 96, height = 48;
    u32 devices = 0;
    engine::testing::ForEachDevice([&](rhi::Device& device) {
        devices++;
        FontFolder folder;
        HX_REQUIRE(folder.font && engine::Shader::Find("Text/Distance Field"));
        rhi::TextureHandle color =
            device.CreateTexture({.format = rhi::Format::RGBA8Unorm, .width = width, .height = height, .usage = rhi::TextureRenderTarget | rhi::TextureSampled});
        rhi::TextureHandle depth =
            device.CreateTexture({.format = rhi::Format::D24UnormS8Uint, .width = width, .height = height, .usage = rhi::TextureDepthStencil});
        engine::Graphics::SetDevice(&device);
        engine::Graphics::SetTarget({color, depth, width, height, rhi::Format::RGBA8Unorm, rhi::Format::D24UnormS8Uint});
        ScreenSize(i32(width), i32(height));
        // The camera looks along +z from z = -10, clearing to black.
        GameObject* eye = engine::GameObject::Create("Eye");
        eye->SetTag("MainCamera");
        eye->transform()->SetPosition(Vector3(0, 0, -10));
        engine::Camera* camera = eye->AddComponent<engine::Camera>();
        camera->SetFieldOfView(60);
        camera->SetClearFlags(engine::CameraClearFlags::SolidColor);
        camera->SetBackgroundColor(Color(0, 0, 0, 1));

        engine::Canvas* canvas = MakeCanvas("Canvas");
        auto text = [&](StringView name, Vector2 at, StringView s, Color c) {
            GameObject* g = Element(name, canvas->transform(), at, Vector2(40, 20));
            CanvasText* t = Add<CanvasText>(g);
            t->SetFont(folder.font);
            t->SetFontSize(18);
            t->SetColor(c);
            t->SetText(s);
            return t;
        };
        // Top left: white bars; top right: red with a green outline; bottom left: yellow with a blue underlay.
        text("Plain", Vector2(-24, 12), "IHI", Color(1, 1, 1, 1));
        CanvasText* outlined = text("Outlined", Vector2(24, 12), "HH", Color(1, 0, 0, 1));
        engine::Material* om = outlined->fontMaterial();
        om->SetColor("_OutlineColor", Color(0, 1, 0, 1));
        om->SetFloat("_OutlineWidth", 0.4f);
        CanvasText* under = text("Underlay", Vector2(-24, -12), "HH", Color(1, 1, 0, 1));
        engine::Material* um = under->fontMaterial();
        um->EnableKeyword("UNDERLAY_ON");
        um->SetColor("_UnderlayColor", Color(0, 0, 1, 1));
        um->SetFloat("_UnderlayOffsetX", 0.6f);
        um->SetFloat("_UnderlayOffsetY", -0.6f);
        // A world text: 2 units of text 1 unit before the origin (the camera sees 11.5 units from top to bottom there).
        GameObject* w = engine::GameObject::Create("World");
        w->AddComponent(engine::ClassOf<engine::RectTransform>());
        WorldText* world = Add<WorldText>(w);
        Rt(w)->SetSizeDelta(Vector2(8, 3));
        w->transform()->SetPosition(Vector3(5, -2.5f, 0));
        world->SetFont(folder.font);
        world->SetFontSize(25);
        world->SetColor(Color(0, 1, 1, 1));
        world->SetText("HI");

        engine::canvas::Batch(true);
        engine::canvas::QueueDraws();
        engine::Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(width * height);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        bool drawn = false;
        for (u32 p : pixels)
            drawn = drawn || (p & 0xFFFFFF) != 0;
        HX_CHECK(drawn);
        HX_CHECK(engine::testing::MatchGolden("TextModes", pixels.Data(), width, height, 8));
        DestroyImmediate(canvas->gameObject());
        DestroyImmediate(w);
        DestroyImmediate(eye);
        engine::canvas::Batch(false);
        engine::Graphics::ReleaseResources();
        engine::Graphics::SetTarget({});
        device.Destroy(color);
        device.Destroy(depth);
    });
    std::printf("    %u devices\n", devices);
}
