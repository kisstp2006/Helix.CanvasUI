cmake_minimum_required(VERSION 3.28)
project(Helix.CanvasUI LANGUAGES CXX)

if(NOT COMMAND helix_add_library)
    include("${CMAKE_CURRENT_SOURCE_DIR}/External/Helix.CMake/Helix.cmake")
endif()

# The canvas game UI: a native module (HelixCanvasUI) of components over the engine's canvases and Helix.UI's text,
# a shared library beside the engine's (a static one linked into the app on the Web).
helix_require(Core)
helix_require(Math)
helix_require(Engine)
helix_require(ScriptCore)
helix_require(UI)
include("${HELIX_BINDGEN_ROOT}/CMake/HelixBindgen.cmake")
find_program(HELIX_DOTNET NAMES dotnet HINTS "$ENV{HELIX_DOTNET_ROOT}" "$ENV{DOTNET_ROOT}" REQUIRED)
helix_dependency_dir(engine_dir Engine)
helix_dependency_dir(math_dir Math)
helix_dependency_dir(ui_dir UI)
helix_dependency_dir(font_dir Font)
helix_dependency_dir(shaders_dir Shaders)
helix_dependency_dir(canvas_platform_dir Platform)
set(engine_source "${engine_dir}/Source")
set(canvas_source "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(canvas_artifacts "${CMAKE_BINARY_DIR}/HelixCanvasUI")

file(GLOB_RECURSE canvas_cpp CONFIGURE_DEPENDS "${canvas_source}/Helix/CanvasUI/*.cpp")
file(GLOB_RECURSE canvas_headers CONFIGURE_DEPENDS "${canvas_source}/Helix/CanvasUI/*.h")
if(HELIX_PLATFORM STREQUAL "web")
    add_library(helix_canvasui STATIC ${canvas_cpp} ${canvas_headers})
    target_link_libraries(helix_canvasui PUBLIC Helix::UIEngine Helix::Engine)
else()
    add_library(helix_canvasui SHARED ${canvas_cpp} ${canvas_headers})
    target_link_libraries(helix_canvasui PUBLIC Helix::UIEngine Helix::EngineShared)
    set_target_properties(helix_canvasui PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                                                    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
add_library(Helix::CanvasUI ALIAS helix_canvasui)
set_target_properties(helix_canvasui PROPERTIES OUTPUT_NAME HelixCanvasUI)
target_include_directories(helix_canvasui PUBLIC "${canvas_source}")
helix_apply_options(helix_canvasui)

# The headers whose tagged types C# sees, and the engine's and the UI's they name.
file(GLOB_RECURSE canvas_bound_headers CONFIGURE_DEPENDS "${canvas_source}/Helix/CanvasUI/*.h")
list(FILTER canvas_bound_headers EXCLUDE REGEX "/Implementation/")
set(canvas_references "${engine_source}/Helix/Engine/Object/EntityId.h" "${engine_source}/Helix/Engine/Object/Object.h"
    "${engine_source}/Helix/Engine/GameObject/GameObject.h" "${engine_source}/Helix/Engine/Script/GameBehaviour.h"
    "${engine_source}/Helix/Engine/Script/ScriptableObject.h" "${engine_source}/Helix/Engine/Transform/Transform.h"
    "${engine_source}/Helix/Engine/Transform/RectTransform.h" "${engine_source}/Helix/Engine/Canvas/Canvas.h"
    "${engine_source}/Helix/Engine/Canvas/CanvasRenderer.h" "${engine_source}/Helix/Engine/Sprite/Sprite.h"
    "${engine_source}/Helix/Engine/Events/HelixEvent.h" "${engine_source}/Helix/Engine/Material/Material.h"
    "${engine_source}/Helix/Engine/Mesh/Mesh.h" "${engine_source}/Helix/Engine/Texture/Texture.h"
    "${engine_source}/Helix/Engine/Rendering/Camera.h" "${engine_source}/Helix/Engine/Math/Hash128.h" "${engine_source}/Helix/Engine/Rendering/Pipeline/RenderStates.h"
    "${engine_source}/Helix/Engine/Transform/RectTransformUtility.h" "${engine_source}/Helix/Engine/Time/Time.h"
    "${ui_dir}/Source/Helix.UI.Engine/Helix/UI/Engine/GuiTypes.h" "${ui_dir}/Source/Helix.UI.Engine/Helix/UI/Engine/UIView.h" "${ui_dir}/Source/Helix.UI.Engine/Helix/UI/Engine/FontAsset.h" "${engine_source}/Helix/Engine/Rendering/Renderer.h" "${ui_dir}/Source/Helix/UI/Text/RichText/RichText.h" "${engine_source}/Helix/Engine/Input/Touch.h"
    "${math_dir}/Source/Helix/Math/Vector.h" "${math_dir}/Source/Helix/Math/Color.h" "${math_dir}/Source/Helix/Math/Matrix4x4.h"
    "${math_dir}/Source/Helix/Math/Quaternion.h" "${math_dir}/Source/Helix/Math/Geometry.h" "${canvas_platform_dir}/Source/Helix/Platform/Window/Input.h")
helix_bindgen(helix_canvasui MODULE HelixCanvasUI NAMESPACE HelixEngine.CanvasUI KIND NativeGame API HEADERS ${canvas_bound_headers}
              REFERENCES ${canvas_references} REFERENCE_NAMESPACE HelixEngine
              INCLUDE_ROOTS "${canvas_source}" "${engine_source}" "${math_dir}/Source" "${canvas_platform_dir}/Source"
              "${ui_dir}/Source" "${ui_dir}/Source/Helix.UI.Engine"
              CS_OUTPUT canvas_cs)
set(HELIX_CANVASUI_BINDINGS "${canvas_cs}" CACHE INTERNAL "")

# The C# side: the generated bindings and the C# written by hand in the reference's shape.
set(runtime_dir "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix.CanvasUI.Runtime")
set(canvas_runtime_dll "${canvas_artifacts}/bin/Helix.CanvasUI.Runtime/release/Helix.CanvasUI.Runtime.dll")
file(GLOB_RECURSE canvas_runtime_sources CONFIGURE_DEPENDS "${runtime_dir}/*.cs" "${runtime_dir}/*.csproj")
list(FILTER canvas_runtime_sources EXCLUDE REGEX "/(bin|obj)/")
add_custom_command(
    OUTPUT "${canvas_runtime_dll}"
    COMMAND "${HELIX_DOTNET}" build "${runtime_dir}/Helix.CanvasUI.Runtime.csproj" -c Release --artifacts-path "${canvas_artifacts}" --nologo -v quiet
            "-p:HelixCoreModule=${HELIX_CORE_MODULE}" "-p:HelixScriptingRuntime=${HELIX_SCRIPTING_RUNTIME}" "-p:HelixUIRuntime=${HELIX_UI_RUNTIME}"
            "-p:HelixModuleBindings=${canvas_cs}"
    DEPENDS ${canvas_runtime_sources} "${HELIX_CORE_MODULE}" "${HELIX_UI_RUNTIME}" "${canvas_cs}"
    COMMENT "Building Helix.CanvasUI.Runtime")
add_custom_target(helix_canvasui_runtime ALL DEPENDS "${canvas_runtime_dll}")
add_dependencies(helix_canvasui_runtime helix_core_module helix_ui_runtime helix_canvasui)
set(HELIX_CANVASUI_RUNTIME "${canvas_runtime_dll}" CACHE INTERNAL "")

# The text's C# shaders (Text/Distance Field): an assembly loaded as a shader asset.
set(canvas_shaders_dll "${canvas_artifacts}/bin/Helix.CanvasUI.Shaders/release/Helix.CanvasUI.Shaders.dll")
file(GLOB_RECURSE canvas_shader_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.cs" "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.csproj"
     "${shaders_dir}/Source/*.cs" "${engine_dir}/Shaders/*.cs")
list(FILTER canvas_shader_sources EXCLUDE REGEX "/(bin|obj)/")
add_custom_command(
    OUTPUT "${canvas_shaders_dll}"
    COMMAND "${HELIX_DOTNET}" build "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Helix.CanvasUI.Shaders.csproj" -c Release --artifacts-path "${canvas_artifacts}"
            --nologo -v quiet "-p:HelixShadersDir=${shaders_dir}" "-p:HelixEngineDir=${engine_dir}"
    DEPENDS ${canvas_shader_sources}
    COMMENT "Building the canvas text's C# shaders")
add_custom_target(helix_canvasui_shaders ALL DEPENDS "${canvas_shaders_dll}")
add_dependencies(helix_canvasui helix_canvasui_shaders)
set(HELIX_CANVASUI_SHADERS "${canvas_shaders_dll}" CACHE INTERNAL "")

# Tests in the engine: canvases drawn on every device, the C# tests run by the C++ tests on the .NET host.
if(HELIX_PLATFORM STREQUAL "web")
    helix_add_test_suite(helix_canvasui_tests DOTNET DEPS Helix::CanvasUI Helix::Engine)
else()
    helix_add_test_suite(helix_canvasui_tests DEPS Helix::CanvasUI Helix::EngineShared)
endif()
if(TARGET helix_canvasui_tests)
    set(tests_dir "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
    helix_bindgen(helix_canvasui_tests MODULE CanvasUITests NAMESPACE CanvasUITests KIND NativeGame HEADERS "${tests_dir}/Engine/Support/CppScripts.h"
                  REFERENCES ${canvas_references} ${canvas_bound_headers} REFERENCE_NAMESPACE HelixEngine
                  INCLUDE_ROOTS "${tests_dir}" "${canvas_source}" "${engine_source}" "${math_dir}/Source" "${canvas_platform_dir}/Source" "${ui_dir}/Source"
                  "${ui_dir}/Source/Helix.UI.Engine")
    set(tests_generated "${CMAKE_CURRENT_BINARY_DIR}/Generated")
    add_custom_command(
        OUTPUT "${tests_generated}/CanvasUIShadersAssembly.h"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${tests_generated}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${canvas_shaders_dll}" "-DOUTPUT=${tests_generated}/CanvasUIShadersAssembly.h" -DNAME=kCanvasUIShadersAssembly
                -P "${HELIX_EMBED_FILE}"
        DEPENDS "${canvas_shaders_dll}" "${HELIX_EMBED_FILE}"
        COMMENT "Embedding the canvas text's shaders")
    target_sources(helix_canvasui_tests PRIVATE "${tests_generated}/CanvasUIShadersAssembly.h")
    target_include_directories(helix_canvasui_tests PRIVATE "${tests_dir}/Engine" "${tests_dir}" "${engine_dir}/Tests" "${font_dir}/Tests" "${tests_generated}")
    target_compile_definitions(helix_canvasui_tests PRIVATE "HELIX_ENGINE_GOLDEN_DIR=\"${tests_dir}/Golden\"")

    set(canvas_tests_dll "${canvas_artifacts}/bin/Helix.CanvasUI.Tests/release/Helix.CanvasUI.Tests.dll")
    get_filename_component(canvas_tests_bin "${canvas_tests_dll}" DIRECTORY)
    file(GLOB_RECURSE canvas_test_sources CONFIGURE_DEPENDS "${tests_dir}/Managed/*.cs" "${tests_dir}/Managed/*.csproj")
    list(FILTER canvas_test_sources EXCLUDE REGEX "/(bin|obj)/")
    add_custom_command(
        OUTPUT "${canvas_tests_dll}"
        COMMAND "${HELIX_DOTNET}" build "${tests_dir}/Managed/Helix.CanvasUI.Tests.csproj" -c Release --artifacts-path "${canvas_artifacts}" --nologo -v quiet
                "-p:HelixCoreModule=${HELIX_CORE_MODULE}" "-p:HelixScriptingRuntime=${HELIX_SCRIPTING_RUNTIME}" "-p:HelixUIRuntime=${HELIX_UI_RUNTIME}"
                "-p:HelixCanvasUIRuntime=${canvas_runtime_dll}"
        DEPENDS ${canvas_test_sources} "${canvas_runtime_dll}"
        COMMENT "Building the C# tests of Helix.CanvasUI")
    add_custom_target(helix_canvasui_test_scripts DEPENDS "${canvas_tests_dll}")
    add_dependencies(helix_canvasui_test_scripts helix_canvasui_runtime)
    add_dependencies(helix_canvasui_tests helix_canvasui_test_scripts)
    target_compile_definitions(helix_canvasui_tests PRIVATE "HX_TEST_RUNTIME_ASSEMBLY=\"${HELIX_SCRIPTING_RUNTIME}\""
                                                           "HX_TEST_CORE_MODULE=\"${HELIX_CORE_MODULE}\"" "HX_TEST_SCRIPTS=\"${canvas_tests_dll}\""
                                                           "HX_TEST_UI_RUNTIME=\"${canvas_tests_bin}/Helix.UI.Runtime.dll\""
                                                           "HX_TEST_CANVASUI_RUNTIME=\"${canvas_tests_bin}/Helix.CanvasUI.Runtime.dll\"")
    set_property(TARGET helix_canvasui_tests APPEND PROPERTY HELIX_DOTNET_ASSEMBLIES "${HELIX_CORE_MODULE}" ${HELIX_CORE_MODULE_REFERENCES}
                                                                                    "${HELIX_UI_RUNTIME}" "${canvas_runtime_dll}" "${canvas_tests_dll}")
    if(TARGET helix_canvasui_tests_web)
        add_dependencies(helix_canvasui_tests_web helix_canvasui_test_scripts helix_core_module)
    endif()
endif()

# The .NET runtime is never unloaded: LeakSanitizer is told about its modules.
if(TEST helix_canvasui_tests AND HELIX_ENABLE_ASAN AND HELIX_PLATFORM STREQUAL "linux")
    helix_dependency_dir(scripting_dir Scripting)
    set_tests_properties(helix_canvasui_tests PROPERTIES ENVIRONMENT "LSAN_OPTIONS=suppressions=${scripting_dir}/Tests/Support/LeakSuppressions.txt")
endif()

# The public surface of the C# side against its inventory (Tests/Api/Helix.CanvasUI.csv), with Helix.ScriptCore's tool.
if(HELIX_BUILD_TESTS AND PROJECT_IS_TOP_LEVEL AND HELIX_API_INVENTORY AND TARGET helix_canvasui_runtime)
    add_test(NAME helix_canvasui_api COMMAND "${HELIX_DOTNET}" "${HELIX_API_INVENTORY}" --assembly "${canvas_runtime_dll}" --reference "${HELIX_CORE_MODULE}"
             --reference "${HELIX_SCRIPTING_RUNTIME}" --reference "${HELIX_UI_RUNTIME}" --inventory "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Api/Helix.CanvasUI.csv")
    add_dependencies(helix_canvasui_runtime helix_api_inventory)
endif()
