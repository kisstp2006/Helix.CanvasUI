# Helix.CanvasUI

The game UI of the Helix engine made of GameObjects: canvases in the scene (over the screen, before a camera or in the
3D world), graphics, masks, layout, controls, the event system and its input modules, and text, one C++
implementation for C# and C++ scripts.

## Contents

| Path | Content |
|---|---|
| `Source/Helix/CanvasUI/Core/` | `UIBehaviour` (the base of every component), the rebuild loop: `CanvasUpdateRegistry` with its stages, `ICanvasElement`, the components' runtime state taken again by copies (`OnAfterCloneNative`) |
| `Source/Helix/CanvasUI/Rendering/` | `Graphic` and `MaskableGraphic` (color, raycast target and padding, materials and modifiers, the dirty flags, cross fades), `GraphicRegistry`, `VertexHelper` and `UIVertex`, `Image` (simple, sliced, tiled, filled with every method and origin, preserve aspect, alpha hit test) and `RawImage` |
| `Source/Helix/CanvasUI/Masking/` | `Mask` (stencil), `RectMask2D` (clip rect, softness, padding), `MaskUtilities`, `ClipperRegistry`, `Clipping`, `StencilMaterial` |
| `Source/Helix/CanvasUI/Effects/` | `BaseMeshEffect`, `Shadow`, `Outline`, `PositionAsUV1` |
| `Source/Helix/CanvasUI/Layout/` | The layout elements and controllers: `HorizontalLayoutGroup`, `VerticalLayoutGroup`, `GridLayoutGroup`, `LayoutElement`, `ContentSizeFitter`, `AspectRatioFitter`, `LayoutRebuilder`, `LayoutUtility`, `CanvasScaler` |
| `Source/Helix/CanvasUI/Controls/` | `Selectable` (transitions, navigation, states), `Button`, `Toggle` and `ToggleGroup`, `Slider`, `Scrollbar`, `ScrollRect`, `TextInput` (caret, selection, content types, validation, the input method), `Dropdown` (template, items, multi select), `DefaultControls` and `TextDefaultControls` |
| `Source/Helix/CanvasUI/Events/` | `EventSystem`, `BaseInput`, `StandaloneInputModule`, `TouchInputModule`, the event data, `ExecuteEvents` and the handler interfaces, `EventTrigger`, the raycasters (`GraphicRaycaster` on every render mode and in 3D, `PhysicsRaycaster`, `Physics2DRaycaster`), the UI panels' input through the event system |
| `Source/Helix/CanvasUI/Text/` | The text (namespace `HelixEngine.Text`): `TextBase` over Helix.UI's text generator (rich text, wrapping, overflow, auto size, the text info), `CanvasText` on a canvas, `WorldText` in the scene, their sub meshes, the sprite assets, style sheets, color gradients, `TextSettings`, the material managers, the text utilities |
| `Source/Helix.CanvasUI.Runtime/` | The C# side (`Helix.CanvasUI.Runtime`): the generated bindings and C# written by hand: the events of the controls (`HelixEvent` classes), `Navigation`, `ColorBlock`, `SpriteState`, `RaycastResult`, `ExecuteEvents`, `EventTrigger`'s entries, `DefaultControls`' factory, the text info, `TextInput`'s validation (`onValidateInput`, `InputValidator`), `Dropdown`'s options |
| `Shaders/` | The text's C# shader (`Text/Distance Field`: face, outline, underlay, dilate, softness, bold weights, clip rect, stencil) |
| `Tests/Engine/` | Tests of every component on the engine, C# tests (`Tests/Managed/`), golden images on every device (`Tests/Golden/`) |
| `Tests/Api/Helix.CanvasUI.csv` | The public surface of the C# side (Helix.ScriptCore's inventory tool) |

## A canvas from a script

```csharp
var canvas = new GameObject("HUD").AddComponent<Canvas>();
canvas.renderMode = RenderMode.ScreenSpaceOverlay;
canvas.gameObject.AddComponent<CanvasScaler>().uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
canvas.gameObject.AddComponent<GraphicRaycaster>();
new GameObject("EventSystem").AddComponent<StandaloneInputModule>();

GameObject button = TextDefaultControls.CreateButton(new TextDefaultControls.Resources());
button.transform.SetParent(canvas.transform, false);
button.GetComponent<Button>().onClick.AddListener(() => Debug.Log("Clicked"));

var input = TextDefaultControls.CreateInputField(new TextDefaultControls.Resources()).GetComponent<TextInput>();
input.onValidateInput = (text, index, added) => char.ToUpperInvariant(added);
input.onSubmit.AddListener(value => Debug.Log(value));
```

A world space canvas takes the camera that clicks it (`worldCamera`); its graphics are hit through the camera as
turned in 3D. `WorldText` is a renderer of its own in the scene. A C++ script reaches the same components through
the C++ script API (`HelixEngine.CanvasUI/HelixCanvasUIApi.h`), their events as `onClickNative`,
`onValueChangedNative` and the others.

## Build

```
git clone https://github.com/kisstp2006/Helix.CanvasUI
git -C Helix.CanvasUI submodule update --init
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

The .NET 11 SDK is needed for the C# side and the shaders (`HELIX_DOTNET_ROOT` or `DOTNET_ROOT`).

## License

GPL-3.0, see `LICENSE`.
