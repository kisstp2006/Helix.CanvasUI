#pragma once

#include <Helix/Engine/Script/ScriptClass.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Scripting/Bindings/Interop.h>
#include <Helix/Scripting/Host/DotnetHost.h>
#include <Helix/Scripting/Types/Type.h>

#include <bit>
#include <cstdio>

// The canvas UI's module (and the UI's it stands on) and the tests' game module registered, the engine's C# side
// started with both runtimes and the C# tests (once per process), and the notes the C# tests write.
extern "C" void HxBindings_HelixUI_Publish();
extern "C" void HxBindings_HelixCanvasUI_Publish();
HX_BINDINGS_EXPORT void HxBindings_CanvasUITests_Publish();

namespace helix::canvasui::testing
{
    inline Array<String>& Notes()
    {
        static Array<String> notes;
        return notes;
    }

    inline void NoteFromCSharp(const char* text, i32 size) { Notes().Push(String(StringView(text, usize(size)))); }

    // The modules alone (for tests without C#).
    inline bool RegisterModules()
    {
        static int state = 0;
        if (state == 0)
        {
            state = -1;
            HxBindings_HelixUI_Publish();
            HxBindings_HelixCanvasUI_Publish();
            HxBindings_CanvasUITests_Publish();
            String error;
            for (const char* module : {"HelixUI", "HelixCanvasUI", "CanvasUITests"})
                if (!engine::RegisterLinkedGameModule(module, error))
                {
                    std::printf("    %.*s\n", int(error.Size()), error.Data());
                    return false;
                }
            state = 1;
        }
        return state > 0;
    }

    inline bool StartScripts()
    {
        static int state = 0;
        if (state != 0)
            return state > 0;
        state = -1;
        if (!RegisterModules())
            return false;
        using namespace scripting;
        RegisterNativeExport("CanvasUITests.Note", std::bit_cast<const void*>(&NoteFromCSharp));
        String error;
        DotnetHostDesc desc;
        desc.runtimeAssembly = HX_TEST_RUNTIME_ASSEMBLY;
        if (!DotnetHost::Start(desc, error))
        {
            std::printf("    %.*s\n", int(error.Size()), error.Data());
            return false;
        }
        (void)FindType("HelixEngine.Object");
        AssemblyHandle core = DotnetHost::LoadAssembly(HX_TEST_CORE_MODULE, error);
        auto start = std::bit_cast<i32 (*)()>(DotnetHost::GetFunction(core, "HelixEngine.Internal.ScriptBridge", "Start", error));
        auto load = std::bit_cast<i32 (*)(const char*, i32)>(DotnetHost::GetFunction(core, "HelixEngine.Internal.ScriptBridge", "LoadScripts", error));
        if (!start || !load || start() != 0)
        {
            std::printf("    the engine's C# side did not start: %.*s\n", int(error.Size()), error.Data());
            return false;
        }
        for (StringView runtime : {StringView(HX_TEST_UI_RUNTIME), StringView(HX_TEST_CANVASUI_RUNTIME)})
            if (load(runtime.Data(), i32(runtime.Size())) < 0)
            {
                std::printf("    %.*s did not load\n", int(runtime.Size()), runtime.Data());
                return false;
            }
        StringView scripts = HX_TEST_SCRIPTS;
        if (load(scripts.Data(), i32(scripts.Size())) <= 0)
        {
            std::printf("    no C# test scripts were registered\n");
            return false;
        }
        state = 1;
        return true;
    }

    inline const engine::ScriptClass& CSharp(const char* name)
    {
        const engine::ScriptClass* s = engine::FindScriptClass("Helix.CanvasUI.Tests", "Tests", name);
        HX_ASSERT(s != nullptr);
        return *s;
    }

    // The notes since the last call, and whether they are the expected ones (printed when not).
    inline bool NotesAre(std::initializer_list<const char*> expected)
    {
        Array<String>& n = Notes();
        bool ok = n.Size() == expected.size();
        usize i = 0;
        for (const char* e : expected)
        {
            if (i < n.Size() && n[i] != StringView(e))
                ok = false;
            i++;
        }
        if (!ok)
        {
            for (const String& s : n)
                std::printf("    note:     %s\n", s.CStr());
            for (const char* e : expected)
                std::printf("    expected: %s\n", e);
        }
        n.Clear();
        return ok;
    }
}
