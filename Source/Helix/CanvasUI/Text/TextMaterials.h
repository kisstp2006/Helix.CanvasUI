#pragma once

#include <Helix/Engine/Material/Material.h>
#include <Helix/UI/Engine/FontAsset.h>

// The text's materials: each font asset's shared distance field material (Text/Distance Field, its atlas texture set
// by the renderer of each mesh), and the shader's property names and keywords.
namespace helix::canvasui
{
    class TextMaterials
    {
    public:
        static constexpr StringView kDistanceFieldShader = "Text/Distance Field";
        // The font's material (made with the font's padding and atlas size; the canvas UI's default material where the
        // shader is not loaded). Null without a font.
        static engine::Material* FontMaterial(ui::FontAsset* font);
        // Sets the font's material (null: made again from the shader).
        static void SetFontMaterial(ui::FontAsset* font, engine::Material* material);
        // The values a font gives its material: the gradient scale (padding + 1) and the atlas size.
        static void ApplyFont(engine::Material* material, ui::FontAsset* font);
        // Every material made, gone (tests).
        static void Clear();
    };
}
