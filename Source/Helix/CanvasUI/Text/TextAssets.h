#pragma once

#include <Helix/CanvasUI/Rendering/Graphic.h>
#include <Helix/CanvasUI/Text/TextTypes.h>
#include <Helix/Engine/Sprite/Sprite.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/UI/Text/Layout/TextLayout.h>

// The text's assets: sprite assets (<sprite>: images of a sheet by name, index or code point), style sheets (<style>:
// named opening and closing tags), color gradient presets, the text settings (the defaults every text starts from:
// fonts, fallbacks, sprites, styles) and the registries the tags find assets in by their names' hash.
namespace helix::canvasui
{
    // A sprite of a sprite asset: its character (code point, name, glyph) and its glyph (the sheet's rectangle and
    // metrics in sheet pixels), across the bindings (C# has SpriteCharacter and SpriteGlyph classes over them).
    HX_STRUCT(Namespace = "HelixEngine.Text", Internal)
    struct SpriteCharacterData
    {
        HX_FIELD() u32 unicode = 0xFFFE;
        HX_FIELD() u32 glyphIndex = 0;
        HX_FIELD() f32 scale = 1;
        HX_FIELD() i32 hashCode = 0;
    };

    HX_STRUCT(Namespace = "HelixEngine.Text", Internal)
    struct SpriteGlyphData
    {
        HX_FIELD() u32 index = 0;
        HX_FIELD() ui::GlyphMetrics metrics;
        HX_FIELD() ui::GlyphRect glyphRect;
        HX_FIELD() f32 scale = 1;
        HX_FIELD() i32 atlasIndex = 0;
        HX_FIELD() engine::EntityId sprite;
    };

    // The lines of a sprite asset's face (C#'s FaceInfo over it).
    HX_STRUCT(Namespace = "HelixEngine.Text", Internal)
    struct SpriteFaceData
    {
        HX_FIELD() f32 pointSize = 0;
        HX_FIELD() f32 scale = 1;
        HX_FIELD() f32 lineHeight = 0;
        HX_FIELD() f32 ascentLine = 0;
        HX_FIELD() f32 baseline = 0;
        HX_FIELD() f32 descentLine = 0;
    };

    HX_CLASS(Namespace = "HelixEngine.Text")
    class SpriteAsset : public ui::TextAsset
    {
    public:
        HX_PROPERTY() engine::Texture* spriteSheet() const { return sheet_.Get(); }
        void SetSpriteSheet(engine::Texture* value) { sheet_ = engine::PPtr<engine::Texture>(value); }
        // The point size the sprites' metrics are in (0: a sprite is as tall as the text's size).
        HX_FUNCTION(Internal) SpriteFaceData FaceInfoInternal() const { return faceInfo_; }
        HX_FUNCTION(Internal) void SetFaceInfoInternal(SpriteFaceData value) { faceInfo_ = value; }
        HX_FUNCTION(Internal) Array<SpriteAsset*> FallbacksInternal() const;
        HX_FUNCTION(Internal) void SetFallbacksInternal(Span<SpriteAsset* const> assets);

        // The characters and glyphs (C# keeps its lists over these).
        HX_FUNCTION(Internal) i32 CharacterCountInternal() const { return i32(characters_.Size()); }
        HX_FUNCTION(Internal) SpriteCharacterData CharacterInternal(i32 index) const;
        HX_FUNCTION(Internal) String CharacterNameInternal(i32 index) const;
        HX_FUNCTION(Internal) Array<SpriteGlyphData> GlyphsInternal() const;
        HX_FUNCTION(Internal) void SetTablesInternal(Span<const SpriteCharacterData> characters, Span<const SpriteGlyphData> glyphs);
        HX_FUNCTION(Internal) void SetCharacterNameInternal(i32 index, StringView spriteName);
        // A sprite of the sheet (its rectangle in sheet pixels from the bottom left), named, with a code point (0xFFFE:
        // none); the new sprite's index.
        HX_FUNCTION(Internal) i32 AddSpriteInternal(StringView spriteName, u32 unicode, Rect rect, f32 bearingX, f32 bearingY, f32 advance, f32 scale);

        void UpdateLookupTables() {}
        HX_FUNCTION() i32 GetSpriteIndexFromHashcode(i32 hash) const;
        HX_FUNCTION() i32 GetSpriteIndexFromUnicode(u32 unicode) const;
        HX_FUNCTION() i32 GetSpriteIndexFromName(StringView spriteName) const;
        HX_FUNCTION() void SortGlyphTable();
        // The asset (this one or a fallback, depth first) with the sprite, and its index there.
        HX_FUNCTION(Out = "spriteIndex") static SpriteAsset* SearchForSpriteByUnicode(SpriteAsset* spriteAsset, u32 unicode, bool includeFallbacks, i32& spriteIndex);
        HX_FUNCTION(Out = "spriteIndex") static SpriteAsset* SearchForSpriteByHashCode(SpriteAsset* spriteAsset, i32 hash, bool includeFallbacks, i32& spriteIndex);

        // A sprite of a <sprite> tag at a text size: its texture, uvs and metrics (false: none).
        bool Resolve(i32 index, f32 size, ui::TextSprite& out) const;

        struct Character
        {
            SpriteCharacterData data;
            String name;
        };
        struct GlyphEntry
        {
            SpriteGlyphData data;
        };

    private:
        HX_FIELD(SerializeField, Name = "spriteSheet") engine::PPtr<engine::Texture> sheet_;
        SpriteFaceData faceInfo_;
        Array<engine::PPtr<engine::Object>> fallbacks_;
        Array<Character> characters_;
        Array<GlyphEntry> glyphs_;
    };

    // Named styles of a sheet: <style="Name"> stands for the opening tags, </style> for the closing ones.
    HX_CLASS(Namespace = "HelixEngine.Text")
    class TextStyleSheet : public engine::ScriptableObject, public ui::TextStyleSheetLookup
    {
    public:
        HX_FUNCTION(Internal) i32 StyleCountInternal() const { return i32(styles_.Size()); }
        HX_FUNCTION(Internal) String StyleNameInternal(i32 index) const;
        HX_FUNCTION(Internal) String StyleOpeningInternal(i32 index) const;
        HX_FUNCTION(Internal) String StyleClosingInternal(i32 index) const;
        // A style set or added (by its name).
        HX_FUNCTION(Internal) void SetStyleInternal(StringView styleName, StringView opening, StringView closing);
        HX_FUNCTION(Internal) void ClearStylesInternal() { styles_.Clear(); }
        HX_FUNCTION() void RefreshStyles() {}
        // The index of the style of the name's hash (-1: none).
        HX_FUNCTION(Internal) i32 FindStyleInternal(i32 hashCode) const;

        bool Find(StringView name, String& opening, String& closing) const override;

        struct Style
        {
            String name, opening, closing;
        };

    private:
        Array<Style> styles_;
    };

    HX_CLASS(Namespace = "HelixEngine.Text")
    class TextColorGradient : public engine::ScriptableObject
    {
    public:
        HX_PROPERTY() ColorMode colorMode() const { return colorMode_; }
        void SetColorMode(ColorMode value) { colorMode_ = value; }
        HX_FIELD(SerializeField, Name = "topLeft") Color topLeft = Color(1, 1, 1, 1);
        HX_FIELD(SerializeField, Name = "topRight") Color topRight = Color(1, 1, 1, 1);
        HX_FIELD(SerializeField, Name = "bottomLeft") Color bottomLeft = Color(1, 1, 1, 1);
        HX_FIELD(SerializeField, Name = "bottomRight") Color bottomRight = Color(1, 1, 1, 1);
        // The four corners as the mode spreads them.
        VertexGradient Corners() const;

    private:
        HX_FIELD(SerializeField, Name = "colorMode") ColorMode colorMode_ = ColorMode::FourCornersGradient;
    };

    // The defaults of every text (one instance: the asset at the settings path, else one made).
    HX_CLASS(Namespace = "HelixEngine.Text")
    class TextSettings : public engine::ScriptableObject
    {
    public:
        HX_PROPERTY(ReadOnly) static TextSettings* instance();
        HX_FUNCTION(Internal) static void SetInstanceInternal(TextSettings* settings);
        HX_FUNCTION() static TextSettings* LoadDefaultSettings() { return instance(); }
        HX_FUNCTION() static TextSettings* GetSettings() { return instance(); }

        HX_PROPERTY() static ui::FontAsset* defaultFontAsset();
        static void SetDefaultFontAsset(ui::FontAsset* value);
        HX_PROPERTY() static StringView defaultFontAssetPath() { return instance()->fontPath_; }
        static void SetDefaultFontAssetPath(StringView value) { instance()->fontPath_ = String(value); }
        HX_PROPERTY() static f32 defaultFontSize() { return instance()->fontSize_; }
        static void SetDefaultFontSize(f32 value) { instance()->fontSize_ = value; }
        HX_PROPERTY() static f32 defaultTextAutoSizingMinRatio() { return instance()->autoMin_; }
        static void SetDefaultTextAutoSizingMinRatio(f32 value) { instance()->autoMin_ = value; }
        HX_PROPERTY() static f32 defaultTextAutoSizingMaxRatio() { return instance()->autoMax_; }
        static void SetDefaultTextAutoSizingMaxRatio(f32 value) { instance()->autoMax_ = value; }
        HX_PROPERTY() static Vector2 defaultTextMeshProTextContainerSize() { return instance()->worldSize_; }
        static void SetDefaultTextMeshProTextContainerSize(Vector2 value) { instance()->worldSize_ = value; }
        HX_PROPERTY() static Vector2 defaultTextMeshProUITextContainerSize() { return instance()->canvasSize_; }
        static void SetDefaultTextMeshProUITextContainerSize(Vector2 value) { instance()->canvasSize_ = value; }
        HX_PROPERTY() static bool autoSizeTextContainer() { return instance()->autoSizeContainer_; }
        static void SetAutoSizeTextContainer(bool value) { instance()->autoSizeContainer_ = value; }
        HX_PROPERTY() static TextWrappingModes textWrappingMode() { return instance()->wrapping_; }
        static void SetTextWrappingMode(TextWrappingModes value) { instance()->wrapping_ = value; }
        HX_PROPERTY() static bool enableKerning() { return instance()->kerning_; }
        static void SetEnableKerning(bool value) { instance()->kerning_ = value; }
        HX_PROPERTY() static bool enableExtraPadding() { return instance()->extraPadding_; }
        static void SetEnableExtraPadding(bool value) { instance()->extraPadding_ = value; }
        HX_PROPERTY() static bool enableTintAllSprites() { return instance()->tintAllSprites_; }
        static void SetEnableTintAllSprites(bool value) { instance()->tintAllSprites_ = value; }
        HX_PROPERTY() static bool enableParseEscapeCharacters() { return instance()->parseEscapes_; }
        static void SetEnableParseEscapeCharacters(bool value) { instance()->parseEscapes_ = value; }
        HX_PROPERTY() static bool enableRaycastTarget() { return instance()->raycastTarget_; }
        static void SetEnableRaycastTarget(bool value) { instance()->raycastTarget_ = value; }
        HX_PROPERTY() static bool getFontFeaturesAtRuntime() { return instance()->featuresAtRuntime_; }
        static void SetGetFontFeaturesAtRuntime(bool value) { instance()->featuresAtRuntime_ = value; }
        HX_PROPERTY() static i32 missingGlyphCharacter() { return instance()->missingGlyph_; }
        static void SetMissingGlyphCharacter(i32 value) { instance()->missingGlyph_ = value; }
        HX_PROPERTY() static i32 missingCharacterSpriteUnicode() { return instance()->missingSprite_; }
        static void SetMissingCharacterSpriteUnicode(i32 value) { instance()->missingSprite_ = value; }
        HX_PROPERTY() static bool clearDynamicDataOnBuild() { return instance()->clearDynamic_; }
        static void SetClearDynamicDataOnBuild(bool value) { instance()->clearDynamic_ = value; }
        HX_PROPERTY() static bool warningsDisabled() { return instance()->warningsDisabled_; }
        static void SetWarningsDisabled(bool value) { instance()->warningsDisabled_ = value; }
        HX_PROPERTY() static bool enableEmojiSupport() { return instance()->emoji_; }
        static void SetEnableEmojiSupport(bool value) { instance()->emoji_ = value; }
        HX_PROPERTY() static bool isTextObjectScaleStatic() { return instance()->scaleStatic_; }
        static void SetIsTextObjectScaleStatic(bool value) { instance()->scaleStatic_ = value; }
        HX_PROPERTY() static bool matchMaterialPreset() { return instance()->matchPreset_; }
        static void SetMatchMaterialPreset(bool value) { instance()->matchPreset_ = value; }
        HX_PROPERTY() static bool hideSubTextObjects() { return instance()->hideSub_; }
        static void SetHideSubTextObjects(bool value) { instance()->hideSub_ = value; }
        HX_PROPERTY() static bool useModernHangulLineBreakingRules() { return instance()->modernHangul_; }
        static void SetUseModernHangulLineBreakingRules(bool value) { instance()->modernHangul_ = value; }
        HX_PROPERTY() static SpriteAsset* defaultSpriteAsset();
        static void SetDefaultSpriteAsset(SpriteAsset* value);
        HX_PROPERTY() static StringView defaultSpriteAssetPath() { return instance()->spritePath_; }
        static void SetDefaultSpriteAssetPath(StringView value) { instance()->spritePath_ = String(value); }
        HX_PROPERTY() static TextStyleSheet* defaultStyleSheet();
        static void SetDefaultStyleSheet(TextStyleSheet* value);
        HX_PROPERTY() static StringView styleSheetsResourcePath() { return instance()->stylePath_; }
        static void SetStyleSheetsResourcePath(StringView value) { instance()->stylePath_ = String(value); }
        HX_PROPERTY() static StringView defaultColorGradientPresetsPath() { return instance()->gradientPath_; }
        static void SetDefaultColorGradientPresetsPath(StringView value) { instance()->gradientPath_ = String(value); }
        HX_PROPERTY(ReadOnly) static StringView version() { return StringView("1.4.0"); }
        HX_FUNCTION(Internal) static Array<ui::FontAsset*> FallbackFontAssetsInternal();
        HX_FUNCTION(Internal) static void SetFallbackFontAssetsInternal(Span<ui::FontAsset* const> fonts);
        HX_FUNCTION(Internal) static Array<engine::Object*> EmojiFallbackTextAssetsInternal();
        HX_FUNCTION(Internal) static void SetEmojiFallbackTextAssetsInternal(Span<engine::Object* const> assets);
        HX_FUNCTION() static ui::FontAsset* GetFontAsset() { return defaultFontAsset(); }
        HX_FUNCTION() static SpriteAsset* GetSpriteAsset() { return defaultSpriteAsset(); }
        HX_FUNCTION() static TextStyleSheet* GetStyleSheet() { return defaultStyleSheet(); }
        HX_FUNCTION() static void LoadLinebreakingRules() {}

    private:
        HX_FIELD(SerializeField, Name = "m_defaultFontAsset") engine::PPtr<engine::Object> font_;
        HX_FIELD(SerializeField, Name = "m_defaultFontAssetPath") String fontPath_ = String("Fonts & Materials/");
        HX_FIELD(SerializeField, Name = "m_defaultFontSize") f32 fontSize_ = 36;
        HX_FIELD(SerializeField, Name = "m_defaultAutoSizeMinRatio") f32 autoMin_ = 0.5f;
        HX_FIELD(SerializeField, Name = "m_defaultAutoSizeMaxRatio") f32 autoMax_ = 2;
        HX_FIELD(SerializeField, Name = "m_defaultTextMeshProTextContainerSize") Vector2 worldSize_ = Vector2(20, 5);
        HX_FIELD(SerializeField, Name = "m_defaultTextMeshProUITextContainerSize") Vector2 canvasSize_ = Vector2(200, 50);
        HX_FIELD(SerializeField, Name = "m_autoSizeTextContainer") bool autoSizeContainer_ = false;
        HX_FIELD(SerializeField, Name = "m_TextWrappingMode") TextWrappingModes wrapping_ = TextWrappingModes::Normal;
        HX_FIELD(SerializeField, Name = "m_enableKerning") bool kerning_ = true;
        HX_FIELD(SerializeField, Name = "m_enableExtraPadding") bool extraPadding_ = false;
        HX_FIELD(SerializeField, Name = "m_enableTintAllSprites") bool tintAllSprites_ = false;
        HX_FIELD(SerializeField, Name = "m_enableParseEscapeCharacters") bool parseEscapes_ = true;
        HX_FIELD(SerializeField, Name = "m_EnableRaycastTarget") bool raycastTarget_ = true;
        HX_FIELD(SerializeField, Name = "m_GetFontFeaturesAtRuntime") bool featuresAtRuntime_ = true;
        HX_FIELD(SerializeField, Name = "m_missingGlyphCharacter") i32 missingGlyph_ = 0;
        HX_FIELD(SerializeField, Name = "m_MissingCharacterSpriteUnicode") i32 missingSprite_ = 0;
        HX_FIELD(SerializeField, Name = "m_ClearDynamicDataOnBuild") bool clearDynamic_ = true;
        HX_FIELD(SerializeField, Name = "m_warningsDisabled") bool warningsDisabled_ = false;
        HX_FIELD(SerializeField, Name = "m_EnableEmojiSupport") bool emoji_ = true;
        HX_FIELD(SerializeField, Name = "m_IsTextObjectScaleStatic") bool scaleStatic_ = false;
        HX_FIELD(SerializeField, Name = "m_matchMaterialPreset") bool matchPreset_ = true;
        HX_FIELD(SerializeField, Name = "m_HideSubTextObjects") bool hideSub_ = true;
        HX_FIELD(SerializeField, Name = "m_UseModernHangulLineBreakingRules") bool modernHangul_ = false;
        HX_FIELD(SerializeField, Name = "m_defaultSpriteAsset") engine::PPtr<engine::Object> sprite_;
        HX_FIELD(SerializeField, Name = "m_defaultSpriteAssetPath") String spritePath_ = String("Sprite Assets/");
        HX_FIELD(SerializeField, Name = "m_defaultStyleSheet") engine::PPtr<engine::Object> styleSheet_;
        HX_FIELD(SerializeField, Name = "m_StyleSheetsResourcePath") String stylePath_ = String("");
        HX_FIELD(SerializeField, Name = "m_defaultColorGradientPresetsPath") String gradientPath_ = String("Color Gradient Presets/");
        Array<engine::PPtr<engine::Object>> fallbacks_, emojiFallbacks_;
    };

    // The assets the tags name, by their names' hash: fonts, sprite assets, materials and color gradients.
    HX_CLASS(Namespace = "HelixEngine.Text", Static)
    class MaterialReferenceManager
    {
    public:
        HX_FUNCTION() static void AddFontAsset(ui::FontAsset* fontAsset);
        HX_FUNCTION() static void AddSpriteAsset(SpriteAsset* spriteAsset);
        HX_FUNCTION() static void AddSpriteAsset(i32 hashCode, SpriteAsset* spriteAsset);
        HX_FUNCTION() static void AddFontMaterial(i32 hashCode, engine::Material* material);
        HX_FUNCTION() static void AddColorGradientPreset(i32 hashCode, TextColorGradient* spriteAsset);
        HX_FUNCTION(Out = "fontAsset") static bool TryGetFontAsset(i32 hashCode, ui::FontAsset*& fontAsset);
        HX_FUNCTION(Out = "spriteAsset") static bool TryGetSpriteAsset(i32 hashCode, SpriteAsset*& spriteAsset);
        HX_FUNCTION(Out = "material") static bool TryGetMaterial(i32 hashCode, engine::Material*& material);
        HX_FUNCTION(Out = "gradientPreset") static bool TryGetColorGradientPreset(i32 hashCode, TextColorGradient*& gradientPreset);
        // The reference's hash of a name (the tags' lookups).
        HX_FUNCTION(Internal) static i32 HashOf(StringView name);
        static void Clear();
    };

    // The text's materials: masking (stencil) copies, the fallback fonts' copies of a material's properties.
    HX_CLASS(Namespace = "HelixEngine.Text", Static)
    class MaterialManager
    {
    public:
        HX_FUNCTION() static engine::Material* GetStencilMaterial(engine::Material* baseMaterial, i32 stencilID);
        HX_FUNCTION() static void ReleaseStencilMaterial(engine::Material* stencilMaterial);
        HX_FUNCTION() static engine::Material* GetBaseMaterial(engine::Material* stencilMaterial);
        HX_FUNCTION() static engine::Material* SetStencil(engine::Material* material, i32 stencilID);
        HX_FUNCTION() static i32 GetStencilID(engine::GameObject* obj);
        // A copy of the source material's properties for another font's atlas (made once per pair).
        HX_FUNCTION() static engine::Material* GetFallbackMaterial(engine::Material* sourceMaterial, engine::Material* targetMaterial);
        HX_FUNCTION() static void ReleaseFallbackMaterial(engine::Material* fallbackMaterial);
        HX_FUNCTION() static void CleanupFallbackMaterials();
        HX_FUNCTION() static void CopyMaterialPresetProperties(engine::Material* source, engine::Material* destination);
        HX_FUNCTION() static void ClearMaterials();
        HX_FUNCTION() static engine::Material* GetMaterialForRendering(MaskableGraphic* graphic, engine::Material* baseMaterial);
    };
}
