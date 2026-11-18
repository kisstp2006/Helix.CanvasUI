#pragma once

#include <Support/CanvasTestKit.h>
#include <Support/FontWriter.h>

#include <Helix/CanvasUI/Text/TextAssets.h>
#include <Helix/CanvasUI/Text/TextMaterials.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Core/Types/Guid.h>
#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/UI/Engine/FontAsset.h>
#include <Helix/UI/Engine/UIAssets.h>

#include <CanvasUIShadersAssembly.h>

// What the text tests share: a font that lays out and draws the same everywhere and the text's shaders, as assets.
namespace helix::canvasui::testing
{
    // Every printable ASCII character a bar 480 units wide of its own height (space 250): text laid out the same
    // everywhere (no machine's fonts). Units per em 1000, ascender 800, descender -200, line gap 90.
    inline Array<u8> PageFontBytes()
    {
        using namespace font::testing;
        FontSpec s;
        s.glyphs.Resize(2 + 94);
        s.glyphs[1].advance = 250;
        s.cmap.Push({u32(' '), 1});
        for (u32 c = 33; c < 127; c++)
        {
            u32 g = 2 + (c - 33);
            s.glyphs[g].contours.Push(Square(60, 0, 420, i32(260 + (c % 9) * 55)));
            s.glyphs[g].advance = 480;
            s.cmap.Push({c, g});
        }
        s.names = {{String("1"), String("Page Test")}, {String("2"), String("Regular")}};
        return BuildFont(s);
    }

    // The page font as a font asset of a folder of assets (gone with the test's objects).
    struct FontFolder
    {
        String root;
        engine::FolderAssetSource* source = nullptr;
        String file, shaders;
        ui::FontAsset* font = nullptr;

        FontFolder()
        {
            ui::RegisterFontAssetLoader();
            root = platform::JoinPath(platform::TempDirectory(), Format("helix-canvasui-text-{}", Guid::Generate().ToString()));
            platform::CreateDirectories(root);
            Array<u8> bytes = PageFontBytes();
            file = platform::JoinPath(root, "Page.ttf");
            platform::WriteFile(file, Span<const u8>(bytes.Data(), bytes.Size()));
            // The text's shaders as an asset.
            shaders = platform::JoinPath(root, "Helix.CanvasUI.Shaders.dll");
            platform::WriteFile(shaders, Span<const u8>(kCanvasUIShadersAssembly, sizeof(kCanvasUIShadersAssembly)));
            source = new engine::FolderAssetSource(root);
            engine::AddAssetSource(source);
            if (const engine::AssetEntry* e = engine::FindAssetAtPath("Helix.CanvasUI.Shaders.dll"))
                engine::LoadAsset(e->guid);
            if (const engine::AssetEntry* e = engine::FindAssetAtPath("Page.ttf"))
                font = ui::ScriptObjectAs<ui::FontAsset>(engine::LoadAsset(e->guid));
        }

        ~FontFolder()
        {
            engine::testing::Cleanup();
            TextMaterials::Clear();
            MaterialReferenceManager::Clear();
            TextSettings::SetInstanceInternal(nullptr);
            while (engine::PendingDestroyCount())
                engine::ProcessDestroyQueue(), engine::SetLifecycleTime(engine::LifecycleTime() + 1e9);
            engine::SetLifecycleTime(0);
            engine::ResetAssets();
            delete source;
            platform::RemoveFile(file);
            platform::RemoveFile(shaders);
            platform::RemoveEmptyDirectory(root);
        }
    };
}
