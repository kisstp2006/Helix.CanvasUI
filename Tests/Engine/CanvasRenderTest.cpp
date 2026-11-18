#include <Support/Devices.h>
#include <Support/Golden.h>
#include <Support/TextKit.h>

#include <Helix/CanvasUI/Masking/Masks.h>
#include <Helix/CanvasUI/Rendering/Image.h>
#include <Helix/CanvasUI/Text/CanvasText.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Testing/Test.h>

#include <cstdio>

using namespace helix;
using namespace helix::canvasui;
using namespace helix::canvasui::testing;
using engine::GameObject;

namespace
{
    Image* MakeImage(StringView name, engine::Transform* parent, Vector2 at, Vector2 size, engine::Sprite* sprite, Color color)
    {
        GameObject* g = Element(name, parent, at, size);
        Image* i = Add<Image>(g);
        i->SetSprite(sprite);
        i->SetColor(color);
        return i;
    }
}

// The three render modes on every device: an overlay canvas (a sliced image, a raw image, a stencil mask over a
// larger child, a rect mask clipping one, a turned element), a camera canvas behind it and a world canvas turned in
// 3D with text on it.
HX_TEST(CanvasRender, OverlayCameraAndWorldCanvasesDrawOnEveryDevice)
{
    HX_REQUIRE(RegisterModules());
    constexpr u32 width = 128, height = 96;
    u32 devices = 0;
    engine::testing::ForEachDevice([&](rhi::Device& device) {
        devices++;
        FontFolder folder;
        HX_REQUIRE(folder.font);
        rhi::TextureHandle color =
            device.CreateTexture({.format = rhi::Format::RGBA8Unorm, .width = width, .height = height, .usage = rhi::TextureRenderTarget | rhi::TextureSampled});
        rhi::TextureHandle depth =
            device.CreateTexture({.format = rhi::Format::D24UnormS8Uint, .width = width, .height = height, .usage = rhi::TextureDepthStencil});
        engine::Graphics::SetDevice(&device);
        engine::Graphics::SetTarget({color, depth, width, height, rhi::Format::RGBA8Unorm, rhi::Format::D24UnormS8Uint});
        ScreenSize(i32(width), i32(height));
        GameObject* eye = engine::GameObject::Create("Eye");
        eye->SetTag("MainCamera");
        eye->transform()->SetPosition(Vector3(0, 0, -10));
        engine::Camera* camera = eye->AddComponent<engine::Camera>();
        camera->SetFieldOfView(60);
        camera->SetClearFlags(engine::CameraClearFlags::SolidColor);
        camera->SetBackgroundColor(Color(0, 0, 0, 1));

        engine::Texture2D* framed = FramedTexture(16, 3, Color32(255, 255, 255, 255), Color32(255, 0, 0, 255));
        engine::Sprite* sliced = SpriteOf(framed, Vector4(3, 3, 3, 3));
        engine::Texture2D* checker = FramedTexture(8, 2, Color32(0, 255, 0, 255), Color32(0, 0, 255, 255));

        // Behind: a camera canvas 5 before the camera with a dark gray band over its bottom.
        engine::Canvas* behind = MakeCanvas("Camera Canvas", engine::RenderMode::ScreenSpaceCamera);
        behind->SetWorldCamera(camera);
        behind->SetPlaneDistance(5);
        Image* band = MakeImage("Band", behind->transform(), Vector2(0, -40), Vector2(128, 16), nullptr, Color(0.3f, 0.3f, 0.3f, 1));
        (void)band;

        // In 3D: a world canvas 60 x 30 units at a hundredth of a unit per unit, turned 40 degrees about y, with a
        // yellow panel and cyan text.
        engine::Canvas* world = MakeCanvas("World Canvas", engine::RenderMode::WorldSpace);
        world->SetWorldCamera(camera);
        Rt(world->gameObject())->SetSizeDelta(Vector2(60, 30));
        world->transform()->SetPosition(Vector3(2.5f, 0.5f, 0));
        world->transform()->SetRotation(Quaternion::Euler(0, 40, 0));
        world->transform()->SetLocalScale(Vector3(0.05f, 0.05f, 0.05f));
        MakeImage("Panel", world->transform(), Vector2(0, 0), Vector2(60, 30), nullptr, Color(0.8f, 0.8f, 0, 1));
        GameObject* label = Element("Label", world->transform(), Vector2(0, 0), Vector2(56, 26));
        CanvasText* text = Add<CanvasText>(label);
        text->SetFont(folder.font);
        text->SetFontSize(32);
        text->SetColor(Color(0, 0.6f, 1, 1));
        text->SetText("HI");

        // Over all: the overlay.
        engine::Canvas* overlay = MakeCanvas("Overlay");
        Image* slicedImage = MakeImage("Sliced", overlay->transform(), Vector2(-40, 30), Vector2(40, 24), sliced, Color(1, 1, 1, 1));
        slicedImage->SetType(Image::Type::Sliced);
        GameObject* raw = Element("Raw", overlay->transform(), Vector2(0, 30), Vector2(24, 24));
        Add<RawImage>(raw)->SetTexture(checker);
        // A stencil mask 20 x 20 over a magenta child twice its size.
        Image* maskImage = MakeImage("Mask", overlay->transform(), Vector2(40, 30), Vector2(20, 20), nullptr, Color(1, 1, 1, 1));
        Add<Mask>(maskImage->gameObject())->SetShowMaskGraphic(false);
        MakeImage("Masked", maskImage->transform(), Vector2(5, -5), Vector2(40, 40), nullptr, Color(1, 0, 1, 1));
        // A rect mask 24 x 12 clipping an orange child.
        GameObject* clip = Element("Clip", overlay->transform(), Vector2(-40, -6), Vector2(24, 12));
        Add<RectMask2D>(clip);
        MakeImage("Clipped", clip->transform(), Vector2(8, 4), Vector2(30, 20), nullptr, Color(1, 0.5f, 0, 1));
        // A white square turned 30 degrees.
        Image* turned = MakeImage("Turned", overlay->transform(), Vector2(-4, -12), Vector2(16, 16), nullptr, Color(1, 1, 1, 1));
        turned->transform()->SetLocalRotation(Quaternion::Euler(0, 0, 30));

        engine::canvas::Batch(true);
        engine::canvas::QueueDraws();
        engine::Graphics::RenderFrame();
        Array<u32> pixels;
        pixels.Resize(width * height);
        HX_REQUIRE(device.ReadTexture(color, 0, pixels.Data(), pixels.Size() * 4));
        HX_CHECK(engine::canvas::CanvasCount() == 3);
        HX_CHECK(engine::testing::MatchGolden("CanvasModes", pixels.Data(), width, height, 8));
        DestroyImmediate(overlay->gameObject());
        DestroyImmediate(world->gameObject());
        DestroyImmediate(behind->gameObject());
        DestroyImmediate(eye);
        DestroyImmediate(framed);
        DestroyImmediate(checker);
        engine::canvas::Batch(false);
        engine::Graphics::ReleaseResources();
        engine::Graphics::SetTarget({});
        device.Destroy(color);
        device.Destroy(depth);
    });
    std::printf("    %u devices\n", devices);
}

// The batcher's draws: one for graphics of one material and texture, apart for another texture and for a mask.
HX_TEST(CanvasRender, BatchesFollowMaterialsTexturesAndMasks)
{
    HX_REQUIRE(RegisterModules());
    {
        ScreenSize(200, 100);
        engine::Canvas* canvas = MakeCanvas("Canvas");
        engine::Texture2D* a = FramedTexture(8, 1, Color32(255, 255, 255, 255), Color32(0, 0, 0, 255));
        engine::Texture2D* b = FramedTexture(8, 1, Color32(255, 0, 0, 255), Color32(0, 0, 0, 255));
        engine::Sprite* sa = SpriteOf(a);
        engine::Sprite* sb = SpriteOf(b);
        for (i32 i = 0; i < 4; i++)
            MakeImage("A", canvas->transform(), Vector2(-60 + f32(i) * 20, 20), Vector2(16, 16), sa, Color(1, 1, 1, 1));
        Frame();
        u32 same = engine::canvas::BatchCount(*canvas);
        HX_CHECK(same == 1);
        MakeImage("B", canvas->transform(), Vector2(40, 20), Vector2(16, 16), sb, Color(1, 1, 1, 1));
        Frame();
        u32 twoTextures = engine::canvas::BatchCount(*canvas);
        HX_CHECK(twoTextures == 2);
        Image* maskImage = MakeImage("Mask", canvas->transform(), Vector2(0, -20), Vector2(30, 30), sa, Color(1, 1, 1, 1));
        Add<Mask>(maskImage->gameObject());
        MakeImage("Inside", maskImage->transform(), Vector2(0, 0), Vector2(20, 20), sa, Color(1, 1, 1, 1));
        Frame();
        u32 masked = engine::canvas::BatchCount(*canvas);
        HX_CHECK(masked > twoTextures);
        std::printf("    batches: %u, %u, %u\n", same, twoTextures, masked);
        DestroyImmediate(canvas->gameObject());
        DestroyImmediate(a);
        DestroyImmediate(b);
    }
    engine::testing::Cleanup();
}
