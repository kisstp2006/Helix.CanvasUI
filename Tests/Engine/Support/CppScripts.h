#pragma once

#include <Helix/Core/Strings/Format.h>
// The engine's script API before the canvas UI's (its classes derive from the engine's).
#include <HelixEngine/HelixEngineApi.h>
#include <HelixEngine.UI/HelixUIApi.h>
#include <HelixEngine.CanvasUI/HelixCanvasUIApi.h>

// C++ scripts of the tests on the canvas UI's C++ script API (the test program's game module, CanvasUITests).
namespace helix::canvasui::testing
{
    inline Array<String>& CppNotes()
    {
        static Array<String> notes;
        return notes;
    }
}
