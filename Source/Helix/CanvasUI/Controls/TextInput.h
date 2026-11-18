#pragma once

#include <Helix/CanvasUI/Controls/Sliders.h>
#include <Helix/CanvasUI/Text/TextBase.h>

// The text input field: a selectable that edits the text of its text component. Activated by a click, a submit or
// its selection (applied at the next late update), it takes the frame's keys and characters while focused: caret and
// selection moves (with shift and control), deletion, the clipboard, Enter and Escape by the line type, characters
// validated by the content type, the validator or a C# callback and kept within the limits; the input method composes
// at the caret. A SelectionCaret graphic in the viewport draws the caret (blinking) and the selection; the text
// scrolls in the viewport to keep the caret in view. Positions are the text's UTF-16 indices (as C#'s strings).
namespace helix::canvasui
{
    class TextInput;

    // The caret and the selection of a text input (a graphic under its viewport the input fills).
    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class SelectionCaret : public MaskableGraphic
    {
    public:
        HX_FUNCTION() void Cull(Rect clipRect, bool validRect) override;
        HX_FUNCTION(Protected) void OnPopulateMesh(VertexHelper* vh) override;
        TextInput* input() const;
        void SetInput(TextInput* value);

    private:
        engine::PPtr<engine::Object> input_;
    };

    // Characters of a C#-written validator (an asset): Validate(ref text, ref pos, ch).
    HX_CLASS(Namespace = "HelixEngine.Text", Abstract)
    class InputValidator : public engine::ScriptableObject
    {
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::RectTransform")
    class TextInput : public Selectable,
                      public IUpdateSelectedHandler,
                      public IBeginDragHandler,
                      public IDragHandler,
                      public IEndDragHandler,
                      public IPointerClickHandler,
                      public ISubmitHandler,
                      public IScrollHandler,
                      public ICanvasElement,
                      public ILayoutElement
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        HX_ENUM()
        enum class ContentType : i32
        {
            Standard = 0,
            Autocorrected = 1,
            IntegerNumber = 2,
            DecimalNumber = 3,
            Alphanumeric = 4,
            Name = 5,
            EmailAddress = 6,
            Password = 7,
            Pin = 8,
            Custom = 9,
        };
        HX_ENUM()
        enum class InputType : i32
        {
            Standard = 0,
            AutoCorrect = 1,
            Password = 2,
        };
        HX_ENUM()
        enum class CharacterValidation : i32
        {
            None = 0,
            Digit = 1,
            Integer = 2,
            Decimal = 3,
            Alphanumeric = 4,
            Name = 5,
            Regex = 6,
            EmailAddress = 7,
            CustomValidator = 8,
        };
        HX_ENUM()
        enum class LineType : i32
        {
            SingleLine = 0,
            MultiLineSubmit = 1,
            MultiLineNewline = 2,
        };
        HX_ENUM()
        enum class EditState : i32
        {
            Continue = 0,
            Finish = 1,
        };
        HX_STRUCT(SerializedOnly)
        struct SubmitEvent : engine::HelixEvent<String>
        {
            static constexpr StringView kTypeName = "SubmitEvent";
        };
        HX_STRUCT(SerializedOnly)
        struct OnChangeEvent : engine::HelixEvent<String>
        {
            static constexpr StringView kTypeName = "OnChangeEvent";
        };
        HX_STRUCT(SerializedOnly)
        struct SelectionEvent : engine::HelixEvent<String>
        {
            static constexpr StringView kTypeName = "SelectionEvent";
        };
        HX_STRUCT(SerializedOnly)
        struct TextSelectionEvent : engine::HelixEvent<String, i32, i32>
        {
            static constexpr StringView kTypeName = "TextSelectionEvent";
        };

        TextInput();

        // The text and what shows it.
        HX_PROPERTY() StringView text() const { return text_; }
        void SetText(StringView value) { SetTextValue(value, true); }
        HX_FUNCTION() void SetTextWithoutNotify(StringView input) { SetTextValue(input, false); }
        HX_PROPERTY() TextBase* textComponent() const { return static_cast<TextBase*>(textComponent_.Get()); }
        void SetTextComponent(TextBase* value);
        HX_PROPERTY() engine::RectTransform* textViewport() const { return textViewport_.Get(); }
        void SetTextViewport(engine::RectTransform* value) { textViewport_ = engine::PPtr<engine::RectTransform>(value); }
        HX_PROPERTY() Graphic* placeholder() const { return static_cast<Graphic*>(placeholder_.Get()); }
        void SetPlaceholder(Graphic* value);
        HX_PROPERTY() Scrollbar* verticalScrollbar() const { return static_cast<Scrollbar*>(verticalScrollbar_.Get()); }
        void SetVerticalScrollbar(Scrollbar* value) { verticalScrollbar_ = engine::PPtr<engine::Object>(value); }
        HX_PROPERTY() f32 scrollSensitivity() const { return scrollSensitivity_; }
        void SetScrollSensitivity(f32 value) { scrollSensitivity_ = value; }
        HX_PROPERTY() ui::FontAsset* fontAsset() const;
        void SetFontAsset(ui::FontAsset* value) { SetGlobalFontAsset(value); }
        HX_PROPERTY() f32 pointSize() const;
        void SetPointSize(f32 value) { SetGlobalPointSize(value); }
        HX_FUNCTION() void SetGlobalFontAsset(ui::FontAsset* fontAsset);
        HX_FUNCTION() void SetGlobalPointSize(f32 pointSize);

        // The options.
        HX_PROPERTY() ContentType contentType() const { return contentType_; }
        void SetContentType(ContentType value);
        HX_PROPERTY() LineType lineType() const { return lineType_; }
        void SetLineType(LineType value);
        HX_PROPERTY() InputType inputType() const { return inputType_; }
        void SetInputType(InputType value);
        HX_PROPERTY() CharacterValidation characterValidation() const { return validation_; }
        void SetCharacterValidation(CharacterValidation value);
        // The pattern of CharacterValidation::Regex (serialized; no property in the bindings).
        StringView regexValue() const { return regexValue_; }
        void SetRegexValue(StringView value) { regexValue_ = String(value); }
        HX_PROPERTY(Internal, Name = "inputValidatorInternal") InputValidator* inputValidator() const { return static_cast<InputValidator*>(validator_.Get()); }
        void SetInputValidator(InputValidator* value);
        HX_PROPERTY(Internal, Name = "keyboardTypeInternal") i32 keyboardType() const { return keyboardType_; }
        void SetKeyboardType(i32 value) { keyboardType_ = value; }
        HX_PROPERTY() bool multiLine() const { return lineType_ != LineType::SingleLine; }
        HX_PROPERTY() i32 characterLimit() const { return characterLimit_; }
        void SetCharacterLimit(i32 value);
        HX_PROPERTY() i32 lineLimit() const { return lineLimit_; }
        void SetLineLimit(i32 value) { lineLimit_ = value; }
        HX_PROPERTY() char16_t asteriskChar() const { return asterisk_; }
        void SetAsteriskChar(char16_t value);
        HX_PROPERTY() bool readOnly() const { return readOnly_; }
        void SetReadOnly(bool value) { readOnly_ = value; }
        HX_PROPERTY() bool richText() const { return richText_; }
        void SetRichText(bool value);
        HX_PROPERTY() bool isRichTextEditingAllowed() const { return richTextEditing_; }
        void SetIsRichTextEditingAllowed(bool value) { richTextEditing_ = value; }
        HX_PROPERTY() bool onFocusSelectAll() const { return onFocusSelectAll_; }
        void SetOnFocusSelectAll(bool value) { onFocusSelectAll_ = value; }
        HX_PROPERTY() bool resetOnDeActivation() const { return resetOnDeactivation_; }
        void SetResetOnDeActivation(bool value) { resetOnDeactivation_ = value; }
        HX_PROPERTY() bool restoreOriginalTextOnEscape() const { return restoreOnEscape_; }
        void SetRestoreOriginalTextOnEscape(bool value) { restoreOnEscape_ = value; }
        HX_PROPERTY() bool keepTextSelectionVisible() const { return keepSelection_; }
        void SetKeepTextSelectionVisible(bool value) { keepSelection_ = value; }
        HX_PROPERTY() bool shouldHideMobileInput() const { return hideMobileInput_; }
        void SetShouldHideMobileInput(bool value) { hideMobileInput_ = value; }
        HX_PROPERTY() bool shouldHideSoftKeyboard() const { return hideSoftKeyboard_; }
        void SetShouldHideSoftKeyboard(bool value) { hideSoftKeyboard_ = value; }
        HX_PROPERTY() virtual bool shouldActivateOnSelect() const { return activateOnSelect_; }
        virtual void SetShouldActivateOnSelect(bool value) { activateOnSelect_ = value; }
        HX_PROPERTY() bool isAlert() const { return isAlert_; }
        void SetIsAlert(bool value) { isAlert_ = value; }

        // The caret and the selection.
        HX_PROPERTY() f32 caretBlinkRate() const { return caretBlinkRate_; }
        void SetCaretBlinkRate(f32 value) { caretBlinkRate_ = value; }
        HX_PROPERTY() i32 caretWidth() const { return caretWidth_; }
        void SetCaretWidth(i32 value) { caretWidth_ = value; }
        HX_PROPERTY() Color caretColor() const { return customCaretColor_ ? caretColor_ : (textComponent() ? textComponent()->color() : Color(0, 0, 0, 1)); }
        void SetCaretColor(Color value) { caretColor_ = value; }
        HX_PROPERTY() bool customCaretColor() const { return customCaretColor_; }
        void SetCustomCaretColor(bool value) { customCaretColor_ = value; }
        HX_PROPERTY() Color selectionColor() const { return selectionColor_; }
        void SetSelectionColor(Color value) { selectionColor_ = value; }
        HX_PROPERTY() i32 caretPosition() const { return focus_; }
        void SetCaretPosition(i32 value);
        HX_PROPERTY() i32 stringPosition() const { return focus_; }
        void SetStringPosition(i32 value) { SetCaretPosition(value); }
        HX_PROPERTY() i32 selectionAnchorPosition() const { return anchor_; }
        void SetSelectionAnchorPosition(i32 value);
        HX_PROPERTY() i32 selectionFocusPosition() const { return focus_; }
        void SetSelectionFocusPosition(i32 value);
        HX_PROPERTY() i32 selectionStringAnchorPosition() const { return anchor_; }
        void SetSelectionStringAnchorPosition(i32 value) { SetSelectionAnchorPosition(value); }
        HX_PROPERTY() i32 selectionStringFocusPosition() const { return focus_; }
        void SetSelectionStringFocusPosition(i32 value) { SetSelectionFocusPosition(value); }
        HX_PROPERTY(ReadOnly) bool isFocused() const { return focused_; }
        HX_PROPERTY(ReadOnly) bool wasCanceled() const { return wasCanceled_; }

        // The events.
        HX_EVENT(Internal) engine::Event<String>& onValueChangedNative() { return onValueChanged_.listeners(); }
        HX_EVENT(Internal) engine::Event<String>& onEndEditNative() { return onEndEdit_.listeners(); }
        HX_EVENT(Internal) engine::Event<String>& onSubmitNative() { return onSubmit_.listeners(); }
        HX_EVENT(Internal) engine::Event<String>& onSelectNative() { return onSelect_.listeners(); }
        HX_EVENT(Internal) engine::Event<String>& onDeselectNative() { return onDeselect_.listeners(); }
        HX_EVENT(Internal) engine::Event<String, i32, i32>& onTextSelectionNative() { return onTextSelection_.listeners(); }
        HX_EVENT(Internal) engine::Event<String, i32, i32>& onEndTextSelectionNative() { return onEndTextSelection_.listeners(); }
        OnChangeEvent& onValueChanged() { return onValueChanged_; }
        SubmitEvent& onEndEdit() { return onEndEdit_; }
        SubmitEvent& onSubmit() { return onSubmit_; }
        SelectionEvent& onSelect() { return onSelect_; }
        SelectionEvent& onDeselect() { return onDeselect_; }
        TextSelectionEvent& onTextSelection() { return onTextSelection_; }
        TextSelectionEvent& onEndTextSelection() { return onEndTextSelection_; }
        // A character for C#'s onValidateInput and the validator (0: rejected): what C# answers through these.
        HX_EVENT(Internal) static engine::Event<TextInput*, i32, i32>& validateCharacter();
        HX_FUNCTION(Internal) void SetValidatedInternal(i32 character, StringView text, i32 position);
        HX_FUNCTION(Internal) void SetValidatedCharacterInternal(i32 character) { validated_ = character; }
        HX_FUNCTION(Internal) bool HasManagedValidationInternal() const { return managedValidation_; }
        HX_FUNCTION(Internal) void SetManagedValidationInternal(bool value) { managedValidation_ = value; }

        // Activation (applied at the next late update) and the edits of scripts.
        HX_FUNCTION() void ActivateInputField();
        HX_FUNCTION() void DeactivateInputField(bool clearSelection = false);
        HX_FUNCTION() void ForceLabelUpdate() { UpdateLabel(); }
        HX_FUNCTION() void MoveTextEnd(bool shift);
        HX_FUNCTION() void MoveTextStart(bool shift);
        HX_FUNCTION() void MoveToEndOfLine(bool shift, bool ctrl);
        HX_FUNCTION() void MoveToStartOfLine(bool shift, bool ctrl);
        HX_FUNCTION() void ReleaseSelection();
        HX_FUNCTION() void OnControlClick() {}
        // A key as an event of the frame (the modifiers of the platform); whether it ended the editing.
        bool ProcessKey(platform::KeyCode key, u8 modifiers);
        void ProcessCharacter(u32 codepoint);

        // The event system's events.
        HX_FUNCTION() virtual void OnBeginDrag(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnDrag(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnEndDrag(PointerEventData* eventData) override;
        HX_FUNCTION() void OnPointerDown(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnPointerClick(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnUpdateSelected(BaseEventData* eventData) override;
        HX_FUNCTION() virtual void OnScroll(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnSubmit(BaseEventData* eventData) override;
        HX_FUNCTION() void OnSelect(BaseEventData* eventData) override;
        HX_FUNCTION() void OnDeselect(BaseEventData* eventData) override;
        HX_FUNCTION() void OnMove(AxisEventData* eventData) override;

        // ICanvasElement: the caret and the selection after the text (LatePreRender).
        HX_FUNCTION() virtual void Rebuild(CanvasUpdate update) override;
        HX_FUNCTION() virtual void LayoutComplete() override {}
        HX_FUNCTION() virtual void GraphicUpdateComplete() override {}
        engine::Transform* transform() const override { return engine::Component::transform(); }
        bool IsDestroyed() const override { return UIBehaviour::IsDestroyed(); }
        engine::Object* ElementObject() override { return this; }

        // ILayoutElement: the text component's.
        HX_FUNCTION() virtual void CalculateLayoutInputHorizontal() override {}
        HX_FUNCTION() virtual void CalculateLayoutInputVertical() override {}
        HX_PROPERTY() virtual f32 minWidth() const override { return 0; }
        HX_PROPERTY() virtual f32 preferredWidth() const override;
        HX_PROPERTY() virtual f32 flexibleWidth() const override { return -1; }
        HX_PROPERTY() virtual f32 minHeight() const override { return 0; }
        HX_PROPERTY() virtual f32 preferredHeight() const override;
        HX_PROPERTY() virtual f32 flexibleHeight() const override { return -1; }
        HX_PROPERTY() virtual i32 layoutPriority() const override { return 1; }
        ILayoutElement* AsLayoutElement() override { return this; }

        HX_FUNCTION(Protected) virtual void LateUpdate();
        void OnEnable() override;
        void OnDisable() override;

        // For the caret graphic: its quads in the text component's space, and whether the caret shows now.
        void CaretQuads(Array<Rect>& selection, Rect& caret, bool& caretShown);
        Span<const char16_t> Characters() const { return Span<const char16_t>(chars_.Data(), chars_.Size()); }

    protected:
        bool HandlesMore(EventKind kind) const override;
        void HandleMore(EventKind kind, BaseEventData* data) override;

    private:
        void SetTextValue(StringView value, bool notify);
        void SetChars(Array<char16_t> chars, bool notify);
        void UpdateLabel();
        void Changed();
        void SetSelection(i32 anchor, i32 focus);
        void Insert(StringView text);
        void DeleteSelection();
        bool HasSelection() const { return anchor_ != focus_; }
        String SelectedText() const;
        char16_t Validate(char16_t c);
        void SendEndEdit();
        void Activate();
        void ScrollToCaret();
        i32 WordStart(i32 from) const;
        i32 WordEnd(i32 from) const;
        i32 LineOf(i32 position) const;
        // The text component's character index of a position (and back): the displayed text has the same characters.
        i32 CaretCharacter(i32 position) const;
        void EnsureCaret();
        void MarkCaret();

        HX_FIELD(SerializeField, Name = "m_TextViewport") engine::PPtr<engine::RectTransform> textViewport_;
        HX_FIELD(SerializeField, Name = "m_TextComponent") engine::PPtr<engine::Object> textComponent_;
        HX_FIELD(SerializeField, Name = "m_Placeholder") engine::PPtr<engine::Object> placeholder_;
        HX_FIELD(SerializeField, Name = "m_VerticalScrollbar") engine::PPtr<engine::Object> verticalScrollbar_;
        HX_FIELD(SerializeField, Name = "m_ScrollSensitivity") f32 scrollSensitivity_ = 1;
        HX_FIELD(SerializeField, Name = "m_ContentType") ContentType contentType_ = ContentType::Standard;
        HX_FIELD(SerializeField, Name = "m_InputType") InputType inputType_ = InputType::Standard;
        HX_FIELD(SerializeField, Name = "m_AsteriskChar") i32 asteriskField_ = '*';
        HX_FIELD(SerializeField, Name = "m_KeyboardType") i32 keyboardType_ = 0;
        HX_FIELD(SerializeField, Name = "m_LineType") LineType lineType_ = LineType::SingleLine;
        HX_FIELD(SerializeField, Name = "m_HideMobileInput") bool hideMobileInput_ = false;
        HX_FIELD(SerializeField, Name = "m_HideSoftKeyboard") bool hideSoftKeyboard_ = false;
        HX_FIELD(SerializeField, Name = "m_CharacterValidation") CharacterValidation validation_ = CharacterValidation::None;
        HX_FIELD(SerializeField, Name = "m_RegexValue") String regexValue_;
        HX_FIELD(SerializeField, Name = "m_GlobalPointSize") f32 globalPointSize_ = 14;
        HX_FIELD(SerializeField, Name = "m_CharacterLimit") i32 characterLimit_ = 0;
        HX_FIELD(SerializeField, Name = "m_OnEndEdit") SubmitEvent onEndEdit_;
        HX_FIELD(SerializeField, Name = "m_OnSubmit") SubmitEvent onSubmit_;
        HX_FIELD(SerializeField, Name = "m_OnSelect") SelectionEvent onSelect_;
        HX_FIELD(SerializeField, Name = "m_OnDeselect") SelectionEvent onDeselect_;
        HX_FIELD(SerializeField, Name = "m_OnTextSelection") TextSelectionEvent onTextSelection_;
        HX_FIELD(SerializeField, Name = "m_OnEndTextSelection") TextSelectionEvent onEndTextSelection_;
        HX_FIELD(SerializeField, Name = "m_OnValueChanged") OnChangeEvent onValueChanged_;
        HX_FIELD(SerializeField, Name = "m_CaretColor") Color caretColor_ = Color(50 / 255.0f, 50 / 255.0f, 50 / 255.0f, 1);
        HX_FIELD(SerializeField, Name = "m_CustomCaretColor") bool customCaretColor_ = false;
        HX_FIELD(SerializeField, Name = "m_SelectionColor") Color selectionColor_ = Color(168 / 255.0f, 206 / 255.0f, 255 / 255.0f, 192 / 255.0f);
        HX_FIELD(SerializeField, Name = "m_Text") String text_;
        HX_FIELD(SerializeField, Name = "m_CaretBlinkRate") f32 caretBlinkRate_ = 0.85f;
        HX_FIELD(SerializeField, Name = "m_CaretWidth") i32 caretWidth_ = 1;
        HX_FIELD(SerializeField, Name = "m_ReadOnly") bool readOnly_ = false;
        HX_FIELD(SerializeField, Name = "m_RichText") bool richText_ = true;
        HX_FIELD(SerializeField, Name = "m_GlobalFontAsset") engine::PPtr<engine::Object> globalFont_;
        HX_FIELD(SerializeField, Name = "m_OnFocusSelectAll") bool onFocusSelectAll_ = true;
        HX_FIELD(SerializeField, Name = "m_ResetOnDeActivation") bool resetOnDeactivation_ = true;
        HX_FIELD(SerializeField, Name = "m_RestoreOriginalTextOnEscape") bool restoreOnEscape_ = true;
        HX_FIELD(SerializeField, Name = "m_isRichTextEditingAllowed") bool richTextEditing_ = false;
        HX_FIELD(SerializeField, Name = "m_LineLimit") i32 lineLimit_ = 0;
        HX_FIELD(SerializeField, Name = "m_InputValidator") engine::PPtr<engine::Object> validator_;
        HX_FIELD(SerializeField, Name = "m_ShouldActivateOnSelect") bool activateOnSelect_ = true;
        bool keepSelection_ = false, isAlert_ = false;
        char16_t asterisk_ = u'*';

        Array<char16_t> chars_;
        i32 anchor_ = 0, focus_ = 0;
        bool focused_ = false, activateNext_ = false, wasCanceled_ = false, dragging_ = false, managedValidation_ = false;
        String original_;
        f64 blinkStart_ = 0;
        u64 inputFrame_ = ~u64(0);
        engine::PPtr<engine::Object> caret_;
        // The C# validation's answer.
        i32 validated_ = 0;
        bool validatedText_ = false;
        Array<char16_t> validatedChars_;
        i32 validatedPosition_ = 0;
    };

    // Clicks and selection of a scrollbar of a text input (the input stays selected while its bar is used).
    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class ScrollbarEventHandler : public UIBehaviour, public INativeEventHandler, public IPointerClickHandler, public ISelectHandler, public IDeselectHandler
    {
    public:
        HX_FIELD() bool isSelected = false;
        HX_FUNCTION() void OnPointerClick(PointerEventData* eventData) override { (void)eventData; }
        HX_FUNCTION() void OnSelect(BaseEventData* eventData) override
        {
            (void)eventData;
            isSelected = true;
        }
        HX_FUNCTION() void OnDeselect(BaseEventData* eventData) override
        {
            (void)eventData;
            isSelected = false;
        }
        bool Handles(EventKind kind) const override { return kind == EventKind::PointerClick || kind == EventKind::Select || kind == EventKind::Deselect; }
        void HandleEvent(EventKind kind, BaseEventData* data) override;
        INativeEventHandler* AsEventHandler() override { return this; }
    };
}
