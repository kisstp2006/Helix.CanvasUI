#pragma once

#include <Helix/CanvasUI/Layout/LayoutRebuilder.h>
#include <Helix/CanvasUI/Rendering/Graphic.h>
#include <Helix/CanvasUI/Text/TextAssets.h>
#include <Helix/CanvasUI/Text/TextTypes.h>
#include <Helix/Math/Geometry.h>
#include <Helix/UI/Text/Layout/TextLayout.h>

#include <memory>

// The text component's base (CanvasText on a canvas, WorldText in the scene): rich text laid out by the UI's text
// system in the RectTransform's rectangle less the margins (wrapped, aligned, spaced, auto sized, overflowing by its
// mode), drawn with the font asset's signed distance field material; the meshes and the text info of the last
// generation. Generation runs in the canvases' rebuild when a property changed (or at once in ForceMeshUpdate).
namespace helix::canvasui
{
    HX_CLASS(Namespace = "HelixEngine.Text", Abstract, RequireComponent = "engine::RectTransform")
    class TextBase : public MaskableGraphic, public ILayoutElement
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        TextBase();
        ~TextBase() override;

        // The text.
        HX_PROPERTY() virtual StringView text() const { return text_; }
        virtual void SetText(StringView value);
        HX_FUNCTION(Name = "SetText") void SetTextSync(StringView sourceText, bool syncTextInputBox = true) { (void)syncTextInputBox, SetText(sourceText); }
        // The numbers in {0} to {7} of the format ({0:2}: with two decimals).
        HX_FUNCTION(Name = "SetText") void SetTextFormat(StringView sourceText, f32 arg0, f32 arg1 = 0, f32 arg2 = 0, f32 arg3 = 0, f32 arg4 = 0, f32 arg5 = 0,
                                                       f32 arg6 = 0, f32 arg7 = 0);
        HX_FUNCTION() void SetCharArray(Span<const char16_t> sourceText) { SetCharArray(sourceText, 0, i32(sourceText.Size())); }
        HX_FUNCTION() void SetCharArray(Span<const char16_t> sourceText, i32 start, i32 length);
        HX_PROPERTY() bool isRightToLeftText() const { return isRightToLeft_; }
        void SetIsRightToLeftText(bool value) { Change(isRightToLeft_, value); }
        HX_PROPERTY() bool richText() const { return richText_; }
        void SetRichText(bool value) { Change(richText_, value); }
        HX_PROPERTY() bool parseCtrlCharacters() const { return parseCtrlCharacters_; }
        void SetParseCtrlCharacters(bool value) { Change(parseCtrlCharacters_, value); }
        HX_PROPERTY() bool emojiFallbackSupport() const { return emojiFallbackSupport_; }
        void SetEmojiFallbackSupport(bool value) { Change(emojiFallbackSupport_, value); }

        // The font and its materials.
        HX_PROPERTY() ui::FontAsset* font() const;
        void SetFont(ui::FontAsset* value);
        HX_PROPERTY() virtual engine::Material* fontSharedMaterial() const;
        virtual void SetFontSharedMaterial(engine::Material* value);
        // A copy of the shared material of this text alone (made when read).
        HX_PROPERTY() engine::Material* fontMaterial();
        void SetFontMaterial(engine::Material* value);
        HX_PROPERTY(ReadOnly) Array<engine::Material*> fontSharedMaterials() const;
        HX_PROPERTY(ReadOnly) Array<engine::Material*> fontMaterials();
        HX_PROPERTY() Color32 faceColor();
        void SetFaceColor(Color32 value);
        HX_PROPERTY() Color32 outlineColor();
        void SetOutlineColor(Color32 value);
        HX_PROPERTY() f32 outlineWidth();
        void SetOutlineWidth(f32 value);

        // The assets of the tags: sprites, styles, a gradient preset; the style of the whole text.
        HX_PROPERTY() SpriteAsset* spriteAsset() const { return static_cast<SpriteAsset*>(spriteAsset_.Get()); }
        void SetSpriteAsset(SpriteAsset* value);
        HX_PROPERTY() TextStyleSheet* styleSheet() const { return static_cast<TextStyleSheet*>(styleSheet_.Get()); }
        void SetStyleSheet(TextStyleSheet* value);
        HX_PROPERTY(Internal, Name = "textStyleHashCode") i32 textStyleHash() const { return textStyleHash_; }
        void SetTextStyleHash(i32 value) { Change(textStyleHash_, value); }
        HX_PROPERTY() TextColorGradient* colorGradientPreset() const { return static_cast<TextColorGradient*>(gradientPreset_.Get()); }
        void SetColorGradientPreset(TextColorGradient* value);
        // A font or sprite asset a tag names that nothing else found: C# answers it (OnFontAssetRequest,
        // OnSpriteAssetRequest) through these.
        // Every text's new mesh (C#'s TextEventManager.TEXT_CHANGED_EVENT).
        HX_EVENT(Internal) static engine::Event<TextBase*>& textChanged();
        HX_EVENT(Internal) static engine::Event<i32>& fontAssetRequest();
        HX_EVENT(Internal) static engine::Event<i32>& spriteAssetRequest();
        HX_FUNCTION(Internal) static String RequestNameInternal();
        HX_FUNCTION(Internal) static void SetRequestedFontInternal(ui::FontAsset* font);
        HX_FUNCTION(Internal) static void SetRequestedSpriteInternal(SpriteAsset* sprites);
        // A font by a tag's name (the registry, the settings' path, then the request).
        static ui::FontAsset* FindFontAsset(StringView name);
        static SpriteAsset* FindSpriteAsset(StringView name);

        // Size and style.
        HX_PROPERTY() f32 fontSize() const { return fontSize_; }
        void SetFontSize(f32 value);
        HX_PROPERTY() bool enableAutoSizing() const { return autoSize_; }
        void SetEnableAutoSizing(bool value) { Change(autoSize_, value); }
        HX_PROPERTY() f32 fontSizeMin() const { return fontSizeMin_; }
        void SetFontSizeMin(f32 value) { Change(fontSizeMin_, value); }
        HX_PROPERTY() f32 fontSizeMax() const { return fontSizeMax_; }
        void SetFontSizeMax(f32 value) { Change(fontSizeMax_, value); }
        HX_PROPERTY() f32 characterWidthAdjustment() const { return characterWidthAdjustment_; }
        void SetCharacterWidthAdjustment(f32 value) { Change(characterWidthAdjustment_, value); }
        HX_PROPERTY() f32 lineSpacingAdjustment() const { return lineSpacingAdjustment_; }
        void SetLineSpacingAdjustment(f32 value) { Change(lineSpacingAdjustment_, value); }
        HX_PROPERTY() FontStyles fontStyle() const { return fontStyle_; }
        void SetFontStyle(FontStyles value) { Change(fontStyle_, value); }
        HX_PROPERTY() FontWeight fontWeight() const { return fontWeight_; }
        void SetFontWeight(FontWeight value) { Change(fontWeight_, value); }
        HX_PROPERTY() f32 characterHorizontalScale() const { return characterHorizontalScale_; }
        void SetCharacterHorizontalScale(f32 value) { Change(characterHorizontalScale_, value); }

        // Color.
        void SetColor(Color value) override;
        HX_PROPERTY() f32 alpha() const { return color().a; }
        void SetAlpha(f32 value);
        HX_PROPERTY() bool enableVertexGradient() const { return enableVertexGradient_; }
        void SetEnableVertexGradient(bool value) { Change(enableVertexGradient_, value); }
        HX_PROPERTY() VertexGradient colorGradient() const { return colorGradient_; }
        void SetColorGradient(VertexGradient value);
        HX_PROPERTY() bool overrideColorTags() const { return overrideColorTags_; }
        void SetOverrideColorTags(bool value) { Change(overrideColorTags_, value); }
        HX_PROPERTY() bool tintAllSprites() const { return tintAllSprites_; }
        void SetTintAllSprites(bool value) { Change(tintAllSprites_, value); }

        // Layout in the rectangle.
        HX_PROPERTY() TextAlignmentOptions alignment() const { return alignment_; }
        void SetAlignment(TextAlignmentOptions value) { Change(alignment_, value); }
        HX_PROPERTY() HorizontalAlignmentOptions horizontalAlignment() const { return HorizontalAlignmentOptions(i32(alignment_) & 0xFF); }
        void SetHorizontalAlignment(HorizontalAlignmentOptions value) { SetAlignment(TextAlignmentOptions((i32(alignment_) & 0xFF00) | i32(value))); }
        HX_PROPERTY() VerticalAlignmentOptions verticalAlignment() const { return VerticalAlignmentOptions(i32(alignment_) & 0xFF00); }
        void SetVerticalAlignment(VerticalAlignmentOptions value) { SetAlignment(TextAlignmentOptions((i32(alignment_) & 0xFF) | i32(value))); }
        HX_PROPERTY() f32 characterSpacing() const { return characterSpacing_; }
        void SetCharacterSpacing(f32 value) { Change(characterSpacing_, value); }
        HX_PROPERTY() f32 wordSpacing() const { return wordSpacing_; }
        void SetWordSpacing(f32 value) { Change(wordSpacing_, value); }
        HX_PROPERTY() f32 lineSpacing() const { return lineSpacing_; }
        void SetLineSpacing(f32 value) { Change(lineSpacing_, value); }
        HX_PROPERTY() f32 paragraphSpacing() const { return paragraphSpacing_; }
        void SetParagraphSpacing(f32 value) { Change(paragraphSpacing_, value); }
        HX_PROPERTY() TextWrappingModes textWrappingMode() const { return wrapping_; }
        void SetTextWrappingMode(TextWrappingModes value) { Change(wrapping_, value); }
        HX_PROPERTY(Attributes = "[System.Obsolete(\"The enableWordWrapping property is now obsolete. Please use the textWrappingMode property instead.\")]")
        bool enableWordWrapping() const { return wrapping_ == TextWrappingModes::Normal; }
        void SetEnableWordWrapping(bool value) { SetTextWrappingMode(value ? TextWrappingModes::Normal : TextWrappingModes::NoWrap); }
        HX_PROPERTY() f32 wordWrappingRatios() const { return wordWrappingRatios_; }
        void SetWordWrappingRatios(f32 value) { Change(wordWrappingRatios_, value); }
        HX_PROPERTY(Attributes = "[System.Obsolete(\"The enabledKerning property is now obsolete. Please use the fontFeatures property instead.\")]")
        bool enableKerning() const { return kerning_; }
        void SetEnableKerning(bool value) { Change(kerning_, value); }
        HX_PROPERTY() TextOverflowModes overflowMode() const { return overflow_; }
        void SetOverflowMode(TextOverflowModes value) { Change(overflow_, value); }
        HX_PROPERTY() TextBase* linkedTextComponent() const;
        void SetLinkedTextComponent(TextBase* value);
        HX_PROPERTY() i32 firstVisibleCharacter() const { return firstVisibleCharacter_; }
        void SetFirstVisibleCharacter(i32 value) { Change(firstVisibleCharacter_, value); }
        HX_PROPERTY() i32 maxVisibleCharacters() const { return maxVisibleCharacters_; }
        void SetMaxVisibleCharacters(i32 value) { ChangeVisibility(maxVisibleCharacters_, value); }
        HX_PROPERTY() i32 maxVisibleWords() const { return maxVisibleWords_; }
        void SetMaxVisibleWords(i32 value) { ChangeVisibility(maxVisibleWords_, value); }
        HX_PROPERTY() i32 maxVisibleLines() const { return maxVisibleLines_; }
        void SetMaxVisibleLines(i32 value) { ChangeVisibility(maxVisibleLines_, value); }
        HX_PROPERTY() bool useMaxVisibleDescender() const { return useMaxVisibleDescender_; }
        void SetUseMaxVisibleDescender(bool value) { Change(useMaxVisibleDescender_, value); }
        HX_PROPERTY() i32 pageToDisplay() const { return pageToDisplay_; }
        void SetPageToDisplay(i32 value) { Change(pageToDisplay_, value); }
        HX_PROPERTY() virtual Vector4 margin() const { return margin_; }
        virtual void SetMargin(Vector4 value) { Change(margin_, value); }
        HX_PROPERTY() bool autoSizeTextContainer() const { return autoSizeTextContainer_; }
        virtual void SetAutoSizeTextContainer(bool value);

        // Rendering options.
        HX_PROPERTY() bool extraPadding() const { return extraPadding_; }
        void SetExtraPadding(bool value) { Change(extraPadding_, value); }
        HX_PROPERTY() bool enableCulling() const { return enableCulling_; }
        void SetEnableCulling(bool value) { Change(enableCulling_, value); }
        HX_PROPERTY() bool ignoreVisibility() const { return ignoreVisibility_; }
        void SetIgnoreVisibility(bool value) { ignoreVisibility_ = value; }
        HX_PROPERTY() bool isOrthographic() const { return isOrthographic_; }
        // The size of a point in the text's units (1 on a canvas; a tenth of a unit for a world text not orthographic).
        virtual f32 FontScale() const { return 1; }
        void SetIsOrthographic(bool value) { Change(isOrthographic_, value); }
        HX_PROPERTY() bool isOverlay() const { return isOverlay_; }
        void SetIsOverlay(bool value) { Change(isOverlay_, value); }
        HX_PROPERTY() bool isTextObjectScaleStatic() const { return isTextObjectScaleStatic_; }
        void SetIsTextObjectScaleStatic(bool value) { isTextObjectScaleStatic_ = value; }
        HX_PROPERTY() bool isUsingLegacyAnimationComponent() const { return legacyAnimation_; }
        void SetIsUsingLegacyAnimationComponent(bool value) { legacyAnimation_ = value; }
        HX_PROPERTY() bool isVolumetricText() const { return volumetric_; }
        void SetIsVolumetricText(bool value) { volumetric_ = value; }
        HX_PROPERTY() VertexSortingOrder geometrySortingOrder() const { return sortingOrder_; }
        void SetGeometrySortingOrder(VertexSortingOrder value) { Change(sortingOrder_, value); }
        HX_PROPERTY() TextureMappingOptions horizontalMapping() const { return horizontalMapping_; }
        void SetHorizontalMapping(TextureMappingOptions value) { Change(horizontalMapping_, value); }
        HX_PROPERTY() TextureMappingOptions verticalMapping() const { return verticalMapping_; }
        void SetVerticalMapping(TextureMappingOptions value) { Change(verticalMapping_, value); }
        HX_PROPERTY() f32 mappingUvLineOffset() const { return mappingUvLineOffset_; }
        void SetMappingUvLineOffset(f32 value) { Change(mappingUvLineOffset_, value); }
        HX_PROPERTY() TextRenderFlags renderMode() const { return renderMode_; }
        void SetRenderMode(TextRenderFlags value) { Change(renderMode_, value); }
        HX_PROPERTY() bool vertexBufferAutoSizeReduction() const { return vertexBufferAutoSizeReduction_; }
        void SetVertexBufferAutoSizeReduction(bool value) { vertexBufferAutoSizeReduction_ = value; }
        HX_PROPERTY() bool havePropertiesChanged() const { return changed_; }
        void SetHavePropertiesChanged(bool value);

        // What the last generation found.
        HX_PROPERTY(ReadOnly) bool isTextOverflowing() const { return firstOverflow_ != -1 || overflowing_; }
        HX_PROPERTY(ReadOnly) i32 firstOverflowCharacterIndex() const { return firstOverflow_; }
        HX_PROPERTY(ReadOnly) bool isTextTruncated() const { return truncated_; }
        HX_PROPERTY(ReadOnly) bool isUsingBold() const { return usingBold_; }
        HX_PROPERTY(ReadOnly) Bounds bounds();
        HX_PROPERTY(ReadOnly) Bounds textBounds();
        HX_PROPERTY(ReadOnly, Internal, Name = "textInfoInternal") TextInfo* textInfo();
        HX_PROPERTY(ReadOnly) virtual engine::Mesh* mesh() { return mesh_; }
        HX_PROPERTY(ReadOnly) f32 pixelsPerUnit() const;
        HX_PROPERTY(ReadOnly) f32 maxWidth() const;
        HX_PROPERTY(ReadOnly) f32 maxHeight() const;

        // ILayoutElement: the text's preferred size, nothing flexible.
        HX_FUNCTION() void CalculateLayoutInputHorizontal() override {}
        HX_FUNCTION() void CalculateLayoutInputVertical() override {}
        HX_PROPERTY(ReadOnly) f32 minWidth() const override { return 0; }
        HX_PROPERTY(ReadOnly) f32 preferredWidth() const override;
        HX_PROPERTY(ReadOnly) f32 flexibleWidth() const override { return -1; }
        HX_PROPERTY(ReadOnly) f32 minHeight() const override { return 0; }
        HX_PROPERTY(ReadOnly) f32 preferredHeight() const override;
        HX_PROPERTY(ReadOnly) f32 flexibleHeight() const override { return -1; }
        HX_PROPERTY(ReadOnly) i32 layoutPriority() const override { return 0; }
        HX_PROPERTY(ReadOnly) virtual f32 renderedWidth() { return GetRenderedValues().x; }
        HX_PROPERTY(ReadOnly) virtual f32 renderedHeight() { return GetRenderedValues().y; }
        ILayoutElement* AsLayoutElement() override { return this; }

        // The text laid out now (from the properties), the meshes and the text info updated.
        HX_FUNCTION() virtual void ForceMeshUpdate(bool ignoreActiveState = false, bool forceTextReparsing = false);
        // The text without its tags.
        HX_FUNCTION() virtual String GetParsedText();
        HX_FUNCTION() Vector2 GetPreferredValues();
        HX_FUNCTION() Vector2 GetPreferredValues(f32 width, f32 height);
        HX_FUNCTION() Vector2 GetPreferredValues(StringView text);
        HX_FUNCTION() Vector2 GetPreferredValues(StringView text, f32 width, f32 height);
        HX_FUNCTION() Vector2 GetRenderedValues() { return GetRenderedValues(true); }
        HX_FUNCTION() Vector2 GetRenderedValues(bool onlyVisibleCharacters);
        // Another text laid out with these properties (the component keeps its own).
        HX_FUNCTION(Internal, Name = "GetTextInfoInternal") virtual TextInfo* GetTextInfo(StringView text);
        HX_FUNCTION() virtual void ClearMesh() { ClearMesh(true); }
        HX_FUNCTION() virtual void ClearMesh(bool uploadGeometry);
        HX_FUNCTION() virtual void ComputeMarginSize();
        // The meshes' arrays of the text info (changed by a script) uploaded again.
        HX_FUNCTION(Internal, Name = "UpdateVertexDataInternal") virtual void UpdateVertexData() { UpdateVertexData(VertexDataUpdateFlags::All); }
        HX_FUNCTION(Internal, Name = "UpdateVertexDataInternal") virtual void UpdateVertexData(VertexDataUpdateFlags flags);
        HX_FUNCTION(Name = "UpdateGeometry") virtual void UpdateMeshGeometry(engine::Mesh* mesh, i32 index);
        HX_FUNCTION() virtual void SetVertices(Span<const Vector3> vertices);
        HX_FUNCTION() virtual void UpdateMeshPadding() { SetVerticesDirty(); }
        // After the layout, before the meshes go up (scripts move the vertices).
        HX_EVENT(Internal, Name = "onPreRenderTextNative") engine::Event<TextInfo*>& OnPreRenderText() { return onPreRenderText_; }

        void SetVerticesDirty() override;
        void SetLayoutDirty() override;
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;
        void OnRectTransformDimensionsChange() override;

    protected:
        // The generation into the text info and the meshes (CanvasText and WorldText upload them).
        void GenerateText();
        // Up to the subclass: the meshes to their renderers.
        virtual void UploadMeshes() {}
        // The material of a mesh: the font page's or a sprite's (the subclass's shader).
        virtual engine::Material* MaterialFor(const ui::TextQuad& quad);
        // Screen pixels per local unit where the text is (the distance fields' antialiasing).
        virtual f32 PixelScale() const { return 1; }
        ui::TextGenerationSettings Settings(f32 width, f32 height) const;
        Rect TextArea() const;
        void UpdateFontAtlases();

        template <class T>
        void Change(T& field, const T& value)
        {
            if (field == value)
                return;
            field = value;
            changed_ = true;
            SetVerticesDirty();
            SetLayoutDirty();
        }
        template <class T>
        void ChangeVisibility(T& field, const T& value)
        {
            if (field == value)
                return;
            field = value;
            changed_ = true;
            SetVerticesDirty();
        }

        engine::Mesh* mesh_ = nullptr;
        Array<engine::Mesh*> subMeshes_;
        // Each mesh's texture (a font's atlas page, a sprite's texture, white for the decorations).
        Array<engine::PPtr<engine::Texture>> meshTextures_;
        TextInfo* info_ = nullptr;
        ui::TextInfo layout_;
        ui::TextGenerator generator_;
        engine::Event<TextInfo*> onPreRenderText_;
        bool changed_ = true;

    private:
        String SourceText() const;
        void BuildInfo(const Rect& area);
        void BuildMeshes(const Rect& area);
        bool Hidden(const CharacterInfo& c, i32 index) const;

        HX_FIELD(SerializeField, Name = "m_text") String text_;
        HX_FIELD(SerializeField, Name = "m_isRightToLeft") bool isRightToLeft_ = false;
        HX_FIELD(SerializeField, Name = "m_fontAsset") engine::PPtr<engine::Object> font_;
        HX_FIELD(SerializeField, Name = "m_sharedMaterial") engine::PPtr<engine::Material> sharedMaterial_;
        engine::PPtr<engine::Material> instanceMaterial_;
        HX_FIELD(SerializeField, Name = "m_fontSize") f32 fontSize_ = 36;
        HX_FIELD(SerializeField, Name = "m_fontSizeBase") f32 fontSizeBase_ = 36;
        HX_FIELD(SerializeField, Name = "m_enableAutoSizing") bool autoSize_ = false;
        HX_FIELD(SerializeField, Name = "m_fontSizeMin") f32 fontSizeMin_ = 18;
        HX_FIELD(SerializeField, Name = "m_fontSizeMax") f32 fontSizeMax_ = 72;
        HX_FIELD(SerializeField, Name = "m_charWidthMaxAdj") f32 characterWidthAdjustment_ = 0;
        HX_FIELD(SerializeField, Name = "m_lineSpacingMax") f32 lineSpacingAdjustment_ = 0;
        HX_FIELD(SerializeField, Name = "m_fontStyle") FontStyles fontStyle_ = FontStyles::Normal;
        HX_FIELD(SerializeField, Name = "m_fontWeight") FontWeight fontWeight_ = FontWeight::Regular;
        HX_FIELD(SerializeField, Name = "m_enableVertexGradient") bool enableVertexGradient_ = false;
        HX_FIELD(SerializeField, Name = "m_fontColorGradient") VertexGradient colorGradient_;
        HX_FIELD(SerializeField, Name = "m_overrideHtmlColors") bool overrideColorTags_ = false;
        HX_FIELD(SerializeField, Name = "m_tintAllSprites") bool tintAllSprites_ = false;
        HX_FIELD(SerializeField, Name = "m_textAlignment") TextAlignmentOptions alignment_ = TextAlignmentOptions::TopLeft;
        HX_FIELD(SerializeField, Name = "m_characterSpacing") f32 characterSpacing_ = 0;
        HX_FIELD(SerializeField, Name = "m_characterHorizontalScale") f32 characterHorizontalScale_ = 1;
        HX_FIELD(SerializeField, Name = "m_wordSpacing") f32 wordSpacing_ = 0;
        HX_FIELD(SerializeField, Name = "m_lineSpacing") f32 lineSpacing_ = 0;
        HX_FIELD(SerializeField, Name = "m_paragraphSpacing") f32 paragraphSpacing_ = 0;
        HX_FIELD(SerializeField, Name = "m_TextWrappingMode") TextWrappingModes wrapping_ = TextWrappingModes::Normal;
        HX_FIELD(SerializeField, Name = "m_wordWrappingRatios") f32 wordWrappingRatios_ = 0.4f;
        HX_FIELD(SerializeField, Name = "m_overflowMode") TextOverflowModes overflow_ = TextOverflowModes::Overflow;
        HX_FIELD(SerializeField, Name = "m_linkedTextComponent") engine::PPtr<engine::Object> linked_;
        HX_FIELD(SerializeField, Name = "m_enableKerning") bool kerning_ = true;
        HX_FIELD(SerializeField, Name = "m_enableExtraPadding") bool extraPadding_ = false;
        HX_FIELD(SerializeField, Name = "m_isRichText") bool richText_ = true;
        HX_FIELD(SerializeField, Name = "m_EmojiFallbackSupport") bool emojiFallbackSupport_ = true;
        HX_FIELD(SerializeField, Name = "m_parseCtrlCharacters") bool parseCtrlCharacters_ = true;
        HX_FIELD(SerializeField, Name = "m_isOrthographic") bool isOrthographic_ = false;
        HX_FIELD(SerializeField, Name = "m_isCullingEnabled") bool enableCulling_ = false;
        HX_FIELD(SerializeField, Name = "m_horizontalMapping") TextureMappingOptions horizontalMapping_ = TextureMappingOptions::Character;
        HX_FIELD(SerializeField, Name = "m_verticalMapping") TextureMappingOptions verticalMapping_ = TextureMappingOptions::Character;
        HX_FIELD(SerializeField, Name = "m_uvLineOffset") f32 mappingUvLineOffset_ = 0;
        HX_FIELD(SerializeField, Name = "m_geometrySortingOrder") VertexSortingOrder sortingOrder_ = VertexSortingOrder::Normal;
        HX_FIELD(SerializeField, Name = "m_IsTextObjectScaleStatic") bool isTextObjectScaleStatic_ = false;
        HX_FIELD(SerializeField, Name = "m_VertexBufferAutoSizeReduction") bool vertexBufferAutoSizeReduction_ = false;
        HX_FIELD(SerializeField, Name = "m_useMaxVisibleDescender") bool useMaxVisibleDescender_ = true;
        HX_FIELD(SerializeField, Name = "m_pageToDisplay") i32 pageToDisplay_ = 1;
        HX_FIELD(SerializeField, Name = "m_margin") Vector4 margin_ = Vector4(0, 0, 0, 0);
        HX_FIELD(SerializeField, Name = "m_isUsingLegacyAnimationComponent") bool legacyAnimation_ = false;
        HX_FIELD(SerializeField, Name = "m_isVolumetricText") bool volumetric_ = false;
        HX_FIELD(SerializeField, Name = "m_autoSizeTextContainer") bool autoSizeTextContainer_ = false;
        HX_FIELD(SerializeField, Name = "m_spriteAsset") engine::PPtr<engine::Object> spriteAsset_;
        HX_FIELD(SerializeField, Name = "m_StyleSheet") engine::PPtr<engine::Object> styleSheet_;
        HX_FIELD(SerializeField, Name = "m_TextStyleHashCode") i32 textStyleHash_ = -1183493901;
        HX_FIELD(SerializeField, Name = "m_fontColorGradientPreset") engine::PPtr<engine::Object> gradientPreset_;
        // The style sheets a text looks in: its own, then the settings'.
        struct Sheets : ui::TextStyleSheetLookup
        {
            const TextBase* text = nullptr;
            bool Find(StringView name, String& opening, String& closing) const override;
        };
        Sheets sheets_;
        i32 firstVisibleCharacter_ = 0, maxVisibleCharacters_ = 99999, maxVisibleWords_ = 99999, maxVisibleLines_ = 99999;
        bool ignoreVisibility_ = false, isOverlay_ = false;
        TextRenderFlags renderMode_ = TextRenderFlags::Render;
        // The last generation's findings.
        i32 firstOverflow_ = -1;
        bool overflowing_ = false, truncated_ = false, usingBold_ = false;
        Bounds bounds_, textBounds_;
        bool generating_ = false;
    };
}
