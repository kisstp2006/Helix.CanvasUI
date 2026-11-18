#pragma once

#include <Support/CanvasHost.h>
#include <Support/TestClasses.h>

#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Canvas/CanvasRenderer.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Script/Coroutines.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Engine/Sprite/Sprite.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Engine/Transform/RectTransform.h>

#include <cmath>

// What the canvas UI's tests share: canvases, elements, components of the module's classes, a test sprite.
namespace helix::canvasui::testing
{
    inline bool Near(f32 a, f32 b, f32 eps = 1e-3f) { return std::fabs(a - b) <= eps; }
    inline bool Near(Vector2 a, Vector2 b, f32 eps = 1e-3f) { return Near(a.x, b.x, eps) && Near(a.y, b.y, eps); }
    inline bool Near(Vector3 a, Vector3 b, f32 eps = 1e-3f) { return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps); }

    inline engine::RectTransform* Rt(engine::GameObject* g) { return static_cast<engine::RectTransform*>(g->transform()); }

    template <class T>
    T* Add(engine::GameObject* g)
    {
        return static_cast<T*>(g->AddComponent(scripting::TypeOf<T>()));
    }

    inline engine::Canvas* MakeCanvas(StringView name, engine::RenderMode mode = engine::RenderMode::ScreenSpaceOverlay)
    {
        engine::GameObject* g = engine::GameObject::Create(name);
        engine::Canvas* c = g->AddComponent<engine::Canvas>();
        c->SetRenderMode(mode);
        return c;
    }

    // A RectTransform of the size at the anchored position (anchors and pivot centered) under the parent.
    inline engine::GameObject* Element(StringView name, engine::Transform* parent, Vector2 position, Vector2 size)
    {
        engine::GameObject* g = engine::GameObject::Create(name);
        auto* r = static_cast<engine::RectTransform*>(g->AddComponent(engine::ClassOf<engine::RectTransform>()));
        r->SetParent(parent, false);
        r->SetSizeDelta(size);
        r->SetAnchoredPosition(position);
        return g;
    }

    // A readable texture of the size: a frame of `border` pixels in one color, the inside in another (rows bottom up).
    inline engine::Texture2D* FramedTexture(i32 size, i32 border, Color32 frame, Color32 inside)
    {
        engine::Texture2D* t = engine::Texture2D::Create(size, size, engine::TextureFormat::RGBA32, false, false);
        Array<Color32> pixels;
        for (i32 y = 0; y < size; y++)
            for (i32 x = 0; x < size; x++)
                pixels.Push(x < border || y < border || x >= size - border || y >= size - border ? frame : inside);
        t->SetPixels32(Span<const Color32>(pixels.Data(), pixels.Size()));
        t->SetFilterMode(engine::FilterMode::Point);
        t->Apply();
        return t;
    }

    inline engine::Sprite* SpriteOf(engine::Texture2D* t, Vector4 border = Vector4(0, 0, 0, 0))
    {
        f32 s = f32(t->width());
        return engine::Sprite::Create(t, Rect(0, 0, s, s), Vector2(0.5f, 0.5f), 100, 0, engine::SpriteMeshType::FullRect, border);
    }

    // The screen's size as the host reports it: the cameras' display and the screen the input and raycasts read.
    inline void ScreenSize(i32 width, i32 height)
    {
        engine::Camera::SetDisplaySize(u32(width), u32(height));
        engine::ScreenReport report;
        report.width = width;
        report.height = height;
        engine::Screen::Report(report);
    }

    // The canvases' loop once: the rebuilds (their will render events) and the batching.
    inline void Frame() { engine::canvas::Batch(true); }

    // Frames of input: the events (positions from the bottom left, as the UI's), the scripts' Update (the event
    // system's) and LateUpdate, the canvases' loop, the coroutines of the frame's end.
    struct InputFrames
    {
        // The time moves a quarter second each frame (past the navigation's repeat delays).
        struct StepClock : engine::TimeClock
        {
            double now = 1;
            double Now() override { return now; }
        };
        StepClock clock;
        Array<engine::InputEvent> events;
        Array<u32> composition;

        InputFrames()
        {
            engine::InputFeed::Reset();
            engine::Time::SetClock(&clock);
        }
        ~InputFrames()
        {
            engine::InputFeed::Reset();
            engine::Time::SetClock(nullptr);
            engine::Time::Reset();
            engine::SetLifecycleTime(0);
        }

        InputFrames& Mouse(platform::EventType type, Vector2 at, platform::MouseButton button = platform::MouseButton::Left)
        {
            engine::InputEvent e;
            e.event.type = type;
            e.event.mouse.x = at.x;
            e.event.mouse.y = f32(engine::Screen::height()) - at.y;
            e.event.mouse.button = button;
            events.Push(e);
            return *this;
        }
        InputFrames& Move(Vector2 at) { return Mouse(platform::EventType::MouseMove, at); }
        InputFrames& Down(Vector2 at) { return Mouse(platform::EventType::MouseDown, at); }
        InputFrames& Up(Vector2 at) { return Mouse(platform::EventType::MouseUp, at); }
        InputFrames& Key(platform::EventType type, platform::KeyCode key)
        {
            engine::InputEvent e;
            e.event.type = type;
            e.event.key.key = key;
            e.event.key.scancode = 0;
            events.Push(e);
            return *this;
        }
        InputFrames& Wheel(f32 dx, f32 dy)
        {
            engine::InputEvent e;
            e.event.type = platform::EventType::MouseWheel;
            e.event.wheel.dx = dx;
            e.event.wheel.dy = dy;
            events.Push(e);
            return *this;
        }
        InputFrames& Press(platform::KeyCode key) { return Key(platform::EventType::KeyDown, key); }
        // A key down with modifiers (platform::ModShift, ModControl, ...).
        InputFrames& Press(platform::KeyCode key, u8 modifiers)
        {
            Key(platform::EventType::KeyDown, key);
            events[events.Size() - 1].event.modifiers = modifiers;
            return *this;
        }
        // Text events of the characters (ASCII), as typed.
        InputFrames& Type(StringView text)
        {
            for (char c : text)
            {
                engine::InputEvent e;
                e.event.type = platform::EventType::Text;
                e.event.text.codepoint = u32(u8(c));
                events.Push(e);
            }
            return *this;
        }
        // The input method's composition of the frame (empty when it ended).
        InputFrames& Compose(StringView text)
        {
            composition.Clear();
            for (char c : text)
                composition.Push(u32(u8(c)));
            engine::InputEvent e;
            e.event.type = platform::EventType::Composition;
            events.Push(e);
            return *this;
        }
        InputFrames& Release(platform::KeyCode key) { return Key(platform::EventType::KeyUp, key); }

        void Run()
        {
            clock.now += 0.25;
            engine::Time::BeginFrame();
            engine::InputFeed::SetFrame(Span<const engine::InputEvent>(events.Data(), events.Size()), Span<const u32>(composition.Data(), composition.Size()));
            engine::RunScriptPhase(engine::ScriptMessage::Update);
            // The coroutines waiting after Update (seconds, null yields), as the player's loop runs them.
            engine::RunCoroutines(engine::CoroutinePoint::Update);
            engine::RunScriptPhase(engine::ScriptMessage::LateUpdate);
            // The objects destroyed this frame go, as after the player's update loop.
            engine::SetLifecycleTime(engine::Time::timeAsDouble());
            engine::ProcessDestroyQueue();
            Frame();
            engine::RunCoroutines(engine::CoroutinePoint::EndOfFrame);
            events.Clear();
        }
    };
}
