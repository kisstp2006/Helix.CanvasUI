#include <Helix/CanvasUI/Controls/TextInput.h>

#include <Helix/CanvasUI/Events/EventSystem.h>
#include <Helix/CanvasUI/Text/CanvasText.h>
#include <Helix/CanvasUI/Text/TextUtilities.h>
#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Engine/Input/SystemClipboard.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Engine/Transform/RectTransform.h>

#include <cmath>

namespace helix::canvasui
{
    namespace
    {
        Array<char16_t> ToUtf16(StringView s)
        {
            Array<char16_t> out;
            for (usize i = 0; i < s.Size();)
            {
                u8 b = u8(s[i]);
                u32 c = b, len = 1;
                if (b >= 0xF0 && i + 3 < s.Size())
                    c = ((b & 7u) << 18) | ((u8(s[i + 1]) & 63u) << 12) | ((u8(s[i + 2]) & 63u) << 6) | (u8(s[i + 3]) & 63u), len = 4;
                else if (b >= 0xE0 && i + 2 < s.Size())
                    c = ((b & 15u) << 12) | ((u8(s[i + 1]) & 63u) << 6) | (u8(s[i + 2]) & 63u), len = 3;
                else if (b >= 0xC0 && i + 1 < s.Size())
                    c = ((b & 31u) << 6) | (u8(s[i + 1]) & 63u), len = 2;
                i += len;
                if (c >= 0x10000)
                {
                    c -= 0x10000;
                    out.Push(char16_t(0xD800 + (c >> 10)));
                    out.Push(char16_t(0xDC00 + (c & 0x3FF)));
                }
                else
                    out.Push(char16_t(c));
            }
            return out;
        }

        String ToUtf8(Span<const char16_t> s)
        {
            String out;
            for (usize i = 0; i < s.Size(); i++)
            {
                u32 c = s[i];
                if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.Size())
                    c = 0x10000 + ((c - 0xD800) << 10) + (u32(s[++i]) - 0xDC00);
                if (c < 0x80)
                    out += char(c);
                else if (c < 0x800)
                    out += char(0xC0 | (c >> 6)), out += char(0x80 | (c & 0x3F));
                else if (c < 0x10000)
                    out += char(0xE0 | (c >> 12)), out += char(0x80 | ((c >> 6) & 0x3F)), out += char(0x80 | (c & 0x3F));
                else
                    out += char(0xF0 | (c >> 18)), out += char(0x80 | ((c >> 12) & 0x3F)), out += char(0x80 | ((c >> 6) & 0x3F)), out += char(0x80 | (c & 0x3F));
            }
            return out;
        }

        bool IsLetter(char16_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0xC0 && c < 0x2000 && c != 0xD7 && c != 0xF7); }
        bool IsDigit(char16_t c) { return c >= '0' && c <= '9'; }
        bool IsWordChar(char16_t c) { return IsLetter(c) || IsDigit(c) || c == '_'; }
        char16_t Upper(char16_t c) { return c >= 'a' && c <= 'z' ? char16_t(c - 32) : c; }
        char16_t Lower(char16_t c) { return c >= 'A' && c <= 'Z' ? char16_t(c + 32) : c; }

        f64 Now() { return engine::Time::unscaledTimeAsDouble(); }

        // One character against a pattern of one character class (the validation's regex: [..] with ranges and
        // negation, \d \w \s, ., or a literal; anchors and a trailing quantifier ignored). Others match everything.
        bool MatchesPattern(StringView pattern, char16_t c)
        {
            usize b = 0, e = pattern.Size();
            if (b < e && pattern[b] == '^')
                b++;
            while (e > b && (pattern[e - 1] == '$' || pattern[e - 1] == '+' || pattern[e - 1] == '*' || pattern[e - 1] == '?'))
                e--;
            StringView p = pattern.Substr(b, e - b);
            auto escape = [&](char k) {
                switch (k)
                {
                case 'd': return IsDigit(c);
                case 'w': return IsWordChar(c);
                case 's': return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r';
                default: return c == char16_t(u8(k));
                }
            };
            if (p.Size() == 2 && p[0] == '\\')
                return escape(p[1]);
            if (p.Size() == 1)
                return p[0] == '.' || c == char16_t(u8(p[0]));
            if (p.Size() < 3 || p[0] != '[' || p[p.Size() - 1] != ']')
                return true;
            usize i = 1, last = p.Size() - 1;
            bool negate = i < last && p[i] == '^';
            if (negate)
                i++;
            bool hit = false;
            while (i < last)
            {
                if (p[i] == '\\' && i + 1 < last)
                {
                    hit = hit || escape(p[i + 1]);
                    i += 2;
                    continue;
                }
                char16_t lo = char16_t(u8(p[i]));
                if (i + 2 < last && p[i + 1] == '-')
                {
                    char16_t hi = char16_t(u8(p[i + 2]));
                    hit = hit || (c >= lo && c <= hi);
                    i += 3;
                    continue;
                }
                hit = hit || c == lo;
                i++;
            }
            return negate ? !hit : hit;
        }
    }

    // ------------------------------------------------------------------------------------------------ SelectionCaret

    TextInput* SelectionCaret::input() const { return static_cast<TextInput*>(input_.Get()); }
    void SelectionCaret::SetInput(TextInput* value) { input_ = engine::PPtr<engine::Object>(value); }

    void SelectionCaret::Cull(Rect clipRect, bool validRect)
    {
        // Culled with its input's viewport, not its own (empty) rectangle.
        (void)clipRect;
        if (engine::CanvasRenderer* r = canvasRenderer())
            r->SetCull(!validRect);
    }

    void SelectionCaret::OnPopulateMesh(VertexHelper* vh)
    {
        vh->Clear();
        TextInput* in = input();
        if (!in)
            return;
        Array<Rect> selection;
        Rect caret;
        bool shown = false;
        in->CaretQuads(selection, caret, shown);
        auto quad = [&](const Rect& r, Color c) {
            i32 start = vh->currentVertCount();
            Color32 c32(u8(c.r * 255 + 0.5f), u8(c.g * 255 + 0.5f), u8(c.b * 255 + 0.5f), u8(c.a * 255 + 0.5f));
            vh->AddVert(Vector3(r.xMin(), r.yMin(), 0), c32, Vector4(0, 0, 0, 0));
            vh->AddVert(Vector3(r.xMin(), r.yMax(), 0), c32, Vector4(0, 1, 0, 0));
            vh->AddVert(Vector3(r.xMax(), r.yMax(), 0), c32, Vector4(1, 1, 0, 0));
            vh->AddVert(Vector3(r.xMax(), r.yMin(), 0), c32, Vector4(1, 0, 0, 0));
            vh->AddTriangle(start, start + 1, start + 2);
            vh->AddTriangle(start + 2, start + 3, start);
        };
        for (const Rect& r : selection)
            quad(r, in->selectionColor());
        if (shown)
            quad(caret, in->caretColor());
    }

    // ------------------------------------------------------------------------------------------------ TextInput

    TextInput::TextInput()
    {
        static bool registered = false;
        if (registered)
            return;
        registered = true;
        auto one = [](i32 index, engine::HelixEventStorage* (*storage)(engine::Object*), void (*raise)(engine::Object*, Span<const scripting::Variant>)) {
            engine::NativeEventEntry e;
            e.storage = storage;
            e.raise = raise;
            engine::RegisterNativeEvent(scripting::TypeOf<TextInput>(), index, e);
        };
        one(0, [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<TextInput*>(o)->onValueChanged_; },
            [](engine::Object* o, Span<const scripting::Variant> a) { static_cast<TextInput*>(o)->onValueChanged_.Invoke(a.Empty() ? String() : String(a[0].AsText())); });
        one(1, [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<TextInput*>(o)->onEndEdit_; },
            [](engine::Object* o, Span<const scripting::Variant> a) { static_cast<TextInput*>(o)->onEndEdit_.Invoke(a.Empty() ? String() : String(a[0].AsText())); });
        one(2, [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<TextInput*>(o)->onSubmit_; },
            [](engine::Object* o, Span<const scripting::Variant> a) { static_cast<TextInput*>(o)->onSubmit_.Invoke(a.Empty() ? String() : String(a[0].AsText())); });
        one(3, [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<TextInput*>(o)->onSelect_; },
            [](engine::Object* o, Span<const scripting::Variant> a) { static_cast<TextInput*>(o)->onSelect_.Invoke(a.Empty() ? String() : String(a[0].AsText())); });
        one(4, [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<TextInput*>(o)->onDeselect_; },
            [](engine::Object* o, Span<const scripting::Variant> a) { static_cast<TextInput*>(o)->onDeselect_.Invoke(a.Empty() ? String() : String(a[0].AsText())); });
        one(5, [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<TextInput*>(o)->onTextSelection_; },
            [](engine::Object* o, Span<const scripting::Variant> a) {
                static_cast<TextInput*>(o)->onTextSelection_.Invoke(a.Size() > 0 ? String(a[0].AsText()) : String(), a.Size() > 1 ? i32(a[1].AsInt()) : 0,
                                                                      a.Size() > 2 ? i32(a[2].AsInt()) : 0);
            });
        one(6, [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<TextInput*>(o)->onEndTextSelection_; },
            [](engine::Object* o, Span<const scripting::Variant> a) {
                static_cast<TextInput*>(o)->onEndTextSelection_.Invoke(a.Size() > 0 ? String(a[0].AsText()) : String(), a.Size() > 1 ? i32(a[1].AsInt()) : 0,
                                                                         a.Size() > 2 ? i32(a[2].AsInt()) : 0);
            });
    }

    void TextInput::OnEnable()
    {
        Selectable::OnEnable();
        asterisk_ = char16_t(asteriskField_);
        if (chars_.Empty() && !text_.Empty())
            chars_ = ToUtf16(text_);
        UpdateLabel();
    }

    void TextInput::OnDisable()
    {
        if (focused_)
            DeactivateInputField();
        activateNext_ = false;
        CanvasUpdateRegistry::UnRegisterCanvasElementForRebuildNative(this);
        if (auto* c = static_cast<SelectionCaret*>(caret_.Get()))
            c->SetVerticesDirty();
        Selectable::OnDisable();
    }

    // ------------------------------------------------------------------------------------------------ the text

    void TextInput::SetTextValue(StringView value, bool notify)
    {
        Array<char16_t> next = ToUtf16(value);
        // Validated (when a validation is set) and within the limit.
        if (validation_ != CharacterValidation::None && validation_ != CharacterValidation::CustomValidator)
        {
            Array<char16_t> kept;
            Array<char16_t> saved = chars_;
            chars_.Clear();
            for (char16_t c : next)
            {
                focus_ = anchor_ = i32(chars_.Size());
                char16_t v = Validate(c);
                if (v)
                    chars_.Push(v);
            }
            kept = chars_;
            chars_ = saved;
            next = kept;
        }
        if (characterLimit_ > 0 && i32(next.Size()) > characterLimit_)
            next.Resize(usize(characterLimit_));
        SetChars(std::move(next), notify);
    }

    void TextInput::SetChars(Array<char16_t> chars, bool notify)
    {
        bool same = chars.Size() == chars_.Size();
        for (usize i = 0; same && i < chars.Size(); i++)
            same = chars[i] == chars_[i];
        chars_ = std::move(chars);
        text_ = ToUtf8(Span<const char16_t>(chars_.Data(), chars_.Size()));
        i32 n = i32(chars_.Size());
        anchor_ = anchor_ > n ? n : anchor_;
        focus_ = focus_ > n ? n : focus_;
        if (notify && !same)
            onValueChanged_.Invoke(text_);
        UpdateLabel();
    }

    void TextInput::SetTextComponent(TextBase* value)
    {
        textComponent_ = engine::PPtr<engine::Object>(value);
        UpdateLabel();
    }

    void TextInput::SetPlaceholder(Graphic* value)
    {
        placeholder_ = engine::PPtr<engine::Object>(value);
        UpdateLabel();
    }

    ui::FontAsset* TextInput::fontAsset() const { return textComponent() ? textComponent()->font() : nullptr; }
    f32 TextInput::pointSize() const { return textComponent() ? textComponent()->fontSize() : globalPointSize_; }

    void TextInput::SetGlobalFontAsset(ui::FontAsset* fontAsset)
    {
        globalFont_ = engine::PPtr<engine::Object>(fontAsset);
        if (TextBase* t = textComponent())
            t->SetFont(fontAsset);
        if (auto* p = static_cast<TextBase*>(placeholder_.Get()); p && p->Is<TextBase>())
            p->SetFont(fontAsset);
    }

    void TextInput::SetGlobalPointSize(f32 pointSize)
    {
        globalPointSize_ = pointSize;
        if (TextBase* t = textComponent())
            t->SetFontSize(pointSize);
        if (auto* p = static_cast<Graphic*>(placeholder_.Get()))
            if (TextBase* pt = ComponentOf<CanvasText>(p->gameObject()) ? static_cast<TextBase*>(ComponentOf<CanvasText>(p->gameObject())) : nullptr)
                pt->SetFontSize(pointSize);
    }

    void TextInput::SetRichText(bool value)
    {
        richText_ = value;
        if (TextBase* t = textComponent())
            t->SetRichText(value);
        UpdateLabel();
    }

    void TextInput::SetAsteriskChar(char16_t value)
    {
        asterisk_ = value;
        asteriskField_ = i32(value);
        UpdateLabel();
    }

    void TextInput::SetCharacterLimit(i32 value)
    {
        characterLimit_ = value < 0 ? 0 : value;
        if (characterLimit_ > 0 && i32(chars_.Size()) > characterLimit_)
        {
            Array<char16_t> cut = chars_;
            cut.Resize(usize(characterLimit_));
            SetChars(std::move(cut), true);
        }
    }

    void TextInput::SetContentType(ContentType value)
    {
        contentType_ = value;
        // The presets of the content types (Custom keeps what is set).
        auto set = [&](LineType line, InputType input, i32 keyboard, CharacterValidation validation) {
            lineType_ = line;
            inputType_ = input;
            keyboardType_ = keyboard;
            validation_ = validation;
        };
        switch (value)
        {
        case ContentType::Standard: set(lineType_, InputType::Standard, 0, CharacterValidation::None); break;
        case ContentType::Autocorrected: set(lineType_, InputType::AutoCorrect, 0, CharacterValidation::None); break;
        case ContentType::IntegerNumber: set(LineType::SingleLine, InputType::Standard, 4, CharacterValidation::Integer); break;
        case ContentType::DecimalNumber: set(LineType::SingleLine, InputType::Standard, 2, CharacterValidation::Decimal); break;
        case ContentType::Alphanumeric: set(LineType::SingleLine, InputType::Standard, 1, CharacterValidation::Alphanumeric); break;
        case ContentType::Name: set(LineType::SingleLine, InputType::Standard, 0, CharacterValidation::Name); break;
        case ContentType::EmailAddress: set(LineType::SingleLine, InputType::Standard, 7, CharacterValidation::EmailAddress); break;
        case ContentType::Password: set(LineType::SingleLine, InputType::Password, 0, CharacterValidation::None); break;
        case ContentType::Pin: set(LineType::SingleLine, InputType::Password, 4, CharacterValidation::Digit); break;
        case ContentType::Custom: break;
        }
        UpdateLabel();
    }

    void TextInput::SetLineType(LineType value)
    {
        lineType_ = value;
        if (value != LineType::SingleLine && contentType_ != ContentType::Standard && contentType_ != ContentType::Autocorrected)
            contentType_ = ContentType::Custom;
        if (TextBase* t = textComponent())
            t->SetTextWrappingMode(value == LineType::SingleLine ? TextWrappingModes::NoWrap : TextWrappingModes::Normal);
        UpdateLabel();
    }

    void TextInput::SetInputType(InputType value)
    {
        inputType_ = value;
        contentType_ = ContentType::Custom;
        UpdateLabel();
    }

    void TextInput::SetCharacterValidation(CharacterValidation value)
    {
        validation_ = value;
        contentType_ = ContentType::Custom;
    }

    void TextInput::SetInputValidator(InputValidator* value)
    {
        validator_ = engine::PPtr<engine::Object>(value);
        if (value)
            validation_ = CharacterValidation::CustomValidator;
    }

    void TextInput::UpdateLabel()
    {
        TextBase* t = textComponent();
        if (!t)
            return;
        // The displayed text: the text (asterisks for a password) with the input method's composition at the caret.
        Array<char16_t> shown;
        bool password = inputType_ == InputType::Password;
        for (char16_t c : chars_)
            shown.Push(password ? asterisk_ : c);
        String composition;
        if (focused_)
            if (EventSystem* es = EventSystem::current(); es && es->currentInputModule() && es->currentInputModule()->input())
                composition = es->currentInputModule()->input()->compositionString();
        if (!composition.Empty())
        {
            Array<char16_t> comp = ToUtf16(composition);
            i32 at = focus_ < i32(shown.Size()) ? focus_ : i32(shown.Size());
            for (usize k = 0; k < comp.Size(); k++)
                shown.Insert(usize(at) + k, comp[k]);
        }
        t->SetRichText(richText_ && !password);
        t->SetText(ToUtf8(Span<const char16_t>(shown.Data(), shown.Size())));
        if (Graphic* p = placeholder())
            p->SetEnabled(chars_.Empty() && composition.Empty());
        MarkCaret();
    }

    // ------------------------------------------------------------------------------------------------ validation

    char16_t TextInput::Validate(char16_t c)
    {
        i32 pos = focus_ < anchor_ ? focus_ : anchor_;
        auto has = [&](char16_t x) {
            for (char16_t y : chars_)
                if (y == x)
                    return true;
            return false;
        };
        bool selectedAll = HasSelection() && (focus_ == 0 || anchor_ == 0) && i32(chars_.Size()) == (focus_ > anchor_ ? focus_ : anchor_);
        switch (validation_)
        {
        case CharacterValidation::None:
        case CharacterValidation::CustomValidator: return c;
        case CharacterValidation::Digit: return IsDigit(c) ? c : 0;
        case CharacterValidation::Integer:
            if (IsDigit(c))
                return pos == 0 && !chars_.Empty() && chars_[0] == '-' && !selectedAll ? 0 : c;
            return c == '-' && pos == 0 && (selectedAll || !has('-')) ? c : 0;
        case CharacterValidation::Decimal:
            if (IsDigit(c))
                return pos == 0 && !chars_.Empty() && chars_[0] == '-' && !selectedAll ? 0 : c;
            if (c == '-')
                return pos == 0 && (selectedAll || !has('-')) ? c : 0;
            if (c == '.' || c == ',')
                return !has('.') && !has(',') ? c : 0;
            return 0;
        case CharacterValidation::Alphanumeric: return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || IsDigit(c) ? c : 0;
        case CharacterValidation::Name:
        {
            // Words capitalized: a letter after a space (or at the start) upper case, the others lower case.
            char16_t prev = pos > 0 && usize(pos - 1) < chars_.Size() ? chars_[usize(pos - 1)] : u' ';
            if (IsLetter(c))
                return prev == ' ' ? Upper(c) : Lower(c);
            if (c == '\'')
                return !has('\'') && prev != ' ' && pos > 0 ? c : 0;
            if (c == ' ')
                return prev != ' ' && pos > 0 ? c : 0;
            return 0;
        }
        case CharacterValidation::EmailAddress:
        {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || IsDigit(c))
                return c;
            if (c == '@')
                return !has('@') ? c : 0;
            const char16_t* allowed = u"!#$%&'*+-/=?^_`{|}~";
            for (const char16_t* a = allowed; *a; a++)
                if (*a == c)
                    return c;
            if (c == '.')
            {
                char16_t prev = pos > 0 && usize(pos - 1) < chars_.Size() ? chars_[usize(pos - 1)] : u' ';
                char16_t next = usize(pos) < chars_.Size() ? chars_[usize(pos)] : u' ';
                return prev != '.' && next != '.' ? c : 0;
            }
            return 0;
        }
        case CharacterValidation::Regex: return regexValue_.Empty() || MatchesPattern(regexValue_, c) ? c : 0;
        }
        return c;
    }

    engine::Event<TextInput*, i32, i32>& TextInput::validateCharacter()
    {
        static engine::Event<TextInput*, i32, i32>* e = new engine::Event<TextInput*, i32, i32>();
        return *e;
    }

    void TextInput::SetValidatedInternal(i32 character, StringView text, i32 position)
    {
        validated_ = character;
        validatedText_ = true;
        validatedChars_ = ToUtf16(text);
        validatedPosition_ = position;
    }

    // ------------------------------------------------------------------------------------------------ editing

    void TextInput::Changed()
    {
        text_ = ToUtf8(Span<const char16_t>(chars_.Data(), chars_.Size()));
        onValueChanged_.Invoke(text_);
        UpdateLabel();
        blinkStart_ = Now();
        ScrollToCaret();
    }

    void TextInput::SetSelection(i32 anchor, i32 focus)
    {
        i32 n = i32(chars_.Size());
        anchor_ = anchor < 0 ? 0 : anchor > n ? n : anchor;
        focus_ = focus < 0 ? 0 : focus > n ? n : focus;
        blinkStart_ = Now();
        MarkCaret();
        ScrollToCaret();
        if (HasSelection())
            onTextSelection_.Invoke(text_, focus_ < anchor_ ? focus_ : anchor_, focus_ > anchor_ ? focus_ : anchor_);
    }

    String TextInput::SelectedText() const
    {
        i32 a = focus_ < anchor_ ? focus_ : anchor_, b = focus_ > anchor_ ? focus_ : anchor_;
        return ToUtf8(Span<const char16_t>(chars_.Data() + a, usize(b - a)));
    }

    void TextInput::DeleteSelection()
    {
        i32 a = focus_ < anchor_ ? focus_ : anchor_, b = focus_ > anchor_ ? focus_ : anchor_;
        for (i32 i = b - 1; i >= a; i--)
            chars_.RemoveAt(usize(i));
        anchor_ = focus_ = a;
    }

    void TextInput::Insert(StringView text)
    {
        if (readOnly_)
            return;
        Array<char16_t> add = ToUtf16(text);
        bool changed = false;
        if (HasSelection())
        {
            DeleteSelection();
            changed = true;
        }
        for (char16_t c : add)
        {
            if (c == '\n' || c == '\r')
            {
                if (lineType_ != LineType::MultiLineNewline)
                    continue;
                c = '\n';
                i32 lines = 1;
                for (char16_t x : chars_)
                    lines += x == '\n' ? 1 : 0;
                if (lineLimit_ > 0 && lines >= lineLimit_)
                    continue;
            }
            else if (c < 32 || c == 127)
                continue;
            if (characterLimit_ > 0 && i32(chars_.Size()) >= characterLimit_)
                break;
            // C#'s onValidateInput takes the place of the validation.
            char16_t v = managedValidation_ ? c : Validate(c);
            if (managedValidation_ || validation_ == CharacterValidation::CustomValidator)
            {
                // C#'s onValidateInput or validator (it may change the text and the position).
                validated_ = v;
                validatedText_ = false;
                validateCharacter().Invoke(this, focus_, i32(v));
                v = char16_t(validated_);
                if (validatedText_)
                {
                    chars_ = validatedChars_;
                    focus_ = anchor_ = validatedPosition_;
                    changed = true;
                    continue;
                }
            }
            if (!v)
                continue;
            chars_.Insert(usize(focus_), v);
            focus_++;
            anchor_ = focus_;
            changed = true;
        }
        if (changed)
            Changed();
    }

    void TextInput::ProcessCharacter(u32 codepoint)
    {
        if (readOnly_ || codepoint < 32 || codepoint == 127)
            return;
        String s;
        if (codepoint < 0x80)
            s += char(codepoint);
        else
        {
            char16_t units[2];
            usize n = 1;
            if (codepoint >= 0x10000)
            {
                units[0] = char16_t(0xD800 + ((codepoint - 0x10000) >> 10));
                units[1] = char16_t(0xDC00 + ((codepoint - 0x10000) & 0x3FF));
                n = 2;
            }
            else
                units[0] = char16_t(codepoint);
            s = ToUtf8(Span<const char16_t>(units, n));
        }
        Insert(s);
    }

    i32 TextInput::WordStart(i32 from) const
    {
        i32 i = from;
        while (i > 0 && !IsWordChar(chars_[usize(i - 1)]))
            i--;
        while (i > 0 && IsWordChar(chars_[usize(i - 1)]))
            i--;
        return i;
    }

    i32 TextInput::WordEnd(i32 from) const
    {
        i32 n = i32(chars_.Size()), i = from;
        while (i < n && !IsWordChar(chars_[usize(i)]))
            i++;
        while (i < n && IsWordChar(chars_[usize(i)]))
            i++;
        return i;
    }

    void TextInput::MoveTextEnd(bool shift) { SetSelection(shift ? anchor_ : i32(chars_.Size()), i32(chars_.Size())); }
    void TextInput::MoveTextStart(bool shift) { SetSelection(shift ? anchor_ : 0, 0); }

    void TextInput::MoveToEndOfLine(bool shift, bool ctrl)
    {
        if (ctrl)
            return MoveTextEnd(shift);
        i32 n = i32(chars_.Size()), i = focus_;
        while (i < n && chars_[usize(i)] != '\n')
            i++;
        SetSelection(shift ? anchor_ : i, i);
    }

    void TextInput::MoveToStartOfLine(bool shift, bool ctrl)
    {
        if (ctrl)
            return MoveTextStart(shift);
        i32 i = focus_;
        while (i > 0 && chars_[usize(i - 1)] != '\n')
            i--;
        SetSelection(shift ? anchor_ : i, i);
    }

    void TextInput::ReleaseSelection()
    {
        anchor_ = focus_;
        MarkCaret();
    }

    i32 TextInput::LineOf(i32 position) const
    {
        i32 line = 0;
        for (i32 i = 0; i < position && usize(i) < chars_.Size(); i++)
            line += chars_[usize(i)] == '\n' ? 1 : 0;
        return line;
    }

    bool TextInput::ProcessKey(platform::KeyCode key, u8 modifiers)
    {
        using K = platform::KeyCode;
        bool shift = (modifiers & platform::ModShift) != 0;
        bool ctrl = (modifiers & (platform::ModControl | platform::ModCommand)) != 0;
        i32 n = i32(chars_.Size());
        switch (key)
        {
        case K::Backspace:
            if (readOnly_)
                return false;
            if (HasSelection())
                DeleteSelection();
            else if (focus_ > 0)
            {
                i32 from = ctrl ? WordStart(focus_) : focus_ - ((focus_ > 1 && chars_[usize(focus_ - 1)] >= 0xDC00 && chars_[usize(focus_ - 1)] < 0xE000) ? 2 : 1);
                for (i32 i = focus_ - 1; i >= from; i--)
                    chars_.RemoveAt(usize(i));
                focus_ = anchor_ = from;
            }
            else
                return false;
            Changed();
            return false;
        case K::Delete:
            if (readOnly_)
                return false;
            if (HasSelection())
                DeleteSelection();
            else if (focus_ < n)
            {
                i32 to = ctrl ? WordEnd(focus_) : focus_ + ((chars_[usize(focus_)] >= 0xD800 && chars_[usize(focus_)] < 0xDC00) ? 2 : 1);
                for (i32 i = (to < n ? to : n) - 1; i >= focus_; i--)
                    chars_.RemoveAt(usize(i));
            }
            else
                return false;
            Changed();
            return false;
        case K::Home: MoveToStartOfLine(shift, ctrl); return false;
        case K::End: MoveToEndOfLine(shift, ctrl); return false;
        case K::LeftArrow:
        {
            if (HasSelection() && !shift)
                return SetSelection(focus_ < anchor_ ? focus_ : anchor_, focus_ < anchor_ ? focus_ : anchor_), false;
            i32 to = ctrl ? WordStart(focus_) : focus_ - 1;
            SetSelection(shift ? anchor_ : to, to);
            return false;
        }
        case K::RightArrow:
        {
            if (HasSelection() && !shift)
                return SetSelection(focus_ > anchor_ ? focus_ : anchor_, focus_ > anchor_ ? focus_ : anchor_), false;
            i32 to = ctrl ? WordEnd(focus_) : focus_ + 1;
            SetSelection(shift ? anchor_ : to, to);
            return false;
        }
        case K::UpArrow:
        case K::DownArrow:
        {
            if (!multiLine())
            {
                if (key == K::UpArrow)
                    MoveTextStart(shift);
                else
                    MoveTextEnd(shift);
                return false;
            }
            // The same column on the line above or below.
            i32 lineStart = focus_;
            while (lineStart > 0 && chars_[usize(lineStart - 1)] != '\n')
                lineStart--;
            i32 column = focus_ - lineStart, to = focus_;
            if (key == K::UpArrow)
            {
                if (lineStart == 0)
                    to = 0;
                else
                {
                    i32 prevStart = lineStart - 1;
                    while (prevStart > 0 && chars_[usize(prevStart - 1)] != '\n')
                        prevStart--;
                    to = prevStart + column < lineStart - 1 ? prevStart + column : lineStart - 1;
                }
            }
            else
            {
                i32 lineEnd = focus_;
                while (lineEnd < n && chars_[usize(lineEnd)] != '\n')
                    lineEnd++;
                if (lineEnd >= n)
                    to = n;
                else
                {
                    i32 nextEnd = lineEnd + 1;
                    while (nextEnd < n && chars_[usize(nextEnd)] != '\n')
                        nextEnd++;
                    to = lineEnd + 1 + column < nextEnd ? lineEnd + 1 + column : nextEnd;
                }
            }
            SetSelection(shift ? anchor_ : to, to);
            return false;
        }
        case K::A:
            if (ctrl)
                SetSelection(0, n);
            return false;
        case K::C:
            if (ctrl && inputType_ != InputType::Password && HasSelection())
                engine::SystemClipboard::SetText(SelectedText());
            return false;
        case K::X:
            if (ctrl && !readOnly_ && inputType_ != InputType::Password && HasSelection())
            {
                engine::SystemClipboard::SetText(SelectedText());
                DeleteSelection();
                Changed();
            }
            return false;
        case K::V:
            if (ctrl && !readOnly_)
                Insert(engine::SystemClipboard::Text());
            return false;
        case K::Return:
        case K::KeypadEnter:
            if (lineType_ == LineType::MultiLineNewline && !readOnly_)
            {
                Insert("\n");
                return false;
            }
            // Submit: the submit event, then the end of the editing.
            onSubmit_.Invoke(text_);
            DeactivateInputField();
            return true;
        case K::Escape:
            wasCanceled_ = true;
            if (restoreOnEscape_)
                SetTextValue(original_, true);
            DeactivateInputField();
            return true;
        default: return false;
        }
    }

    // ------------------------------------------------------------------------------------------------ activation

    void TextInput::ActivateInputField()
    {
        if (!textComponent() || !IsActive() || !IsInteractable())
            return;
        if (focused_)
            return;
        activateNext_ = true;
    }

    void TextInput::Activate()
    {
        activateNext_ = false;
        if (focused_ || !textComponent())
            return;
        // The field takes the selection (its keys come as the selected object's updates).
        if (EventSystem* es = EventSystem::current())
            if (es->currentSelectedGameObject() != gameObject())
                es->SetSelectedGameObject(gameObject());
        focused_ = true;
        wasCanceled_ = false;
        original_ = text_;
        if (onFocusSelectAll_)
            SetSelection(0, i32(chars_.Size()));
        else
            SetSelection(i32(chars_.Size()), i32(chars_.Size()));
        // Keys of this frame go to the field from the next.
        inputFrame_ = engine::InputFeed::FrameNumber();
        EnsureCaret();
        UpdateLabel();
    }

    void TextInput::DeactivateInputField(bool clearSelection)
    {
        activateNext_ = false;
        if (!focused_)
            return;
        focused_ = false;
        if (clearSelection || resetOnDeactivation_)
            anchor_ = focus_;
        SendEndEdit();
        UpdateLabel();
    }

    void TextInput::SendEndEdit() { onEndEdit_.Invoke(text_); }

    void TextInput::LateUpdate()
    {
        if (activateNext_)
            Activate();
        if (!focused_)
            return;
        // The input method's caret where the field's is; the caret blinks.
        if (EventSystem* es = EventSystem::current(); es && es->currentInputModule())
            if (BaseInput* in = es->currentInputModule()->input())
            {
                Array<Rect> selection;
                Rect caret;
                bool shown = false;
                CaretQuads(selection, caret, shown);
                if (TextBase* t = textComponent())
                {
                    Vector3 world = t->engine::Component::transform()->TransformPoint(Vector3(caret.x, caret.yMax(), 0));
                    in->SetCompositionCursorPos(Vector2(world.x, world.y));
                }
            }
        MarkCaret();
    }

    // ------------------------------------------------------------------------------------------------ the caret

    void TextInput::EnsureCaret()
    {
        if (caret_.Get())
            return;
        TextBase* t = textComponent();
        if (!t)
            return;
        engine::RectTransform* textRect = t->rectTransform();
        engine::Transform* parent = textRect->parent();
        // A copy of a field has the copy of the original's caret beside its text: that one is taken over.
        for (u32 i = 0; parent && i < parent->childCount(); i++)
        {
            auto* c = ComponentOf<SelectionCaret>(parent->GetChild(i)->gameObject());
            TextInput* owner = c ? c->input() : nullptr;
            if (c && (!owner || owner == this || owner->caret_.Get() != c))
            {
                c->SetInput(this);
                caret_ = engine::PPtr<engine::Object>(c);
                return;
            }
        }
        engine::GameObject* g = engine::GameObject::Create("Caret");
        auto* r = static_cast<engine::RectTransform*>(g->AddComponent(engine::ClassOf<engine::RectTransform>()));
        r->SetParent(parent, false);
        r->SetAnchorMin(textRect->anchorMin());
        r->SetAnchorMax(textRect->anchorMax());
        r->SetPivot(textRect->pivot());
        r->SetSizeDelta(textRect->sizeDelta());
        r->SetAnchoredPosition(textRect->anchoredPosition());
        // Under the text (drawn before it).
        r->SetSiblingIndex(textRect->GetSiblingIndex());
        g->SetLayer(gameObject()->layer());
        auto* c = static_cast<SelectionCaret*>(g->AddComponent(scripting::TypeOf<SelectionCaret>()));
        if (c)
        {
            c->SetInput(this);
            c->SetRaycastTarget(false);
        }
        caret_ = engine::PPtr<engine::Object>(c);
    }

    void TextInput::MarkCaret()
    {
        if (auto* c = static_cast<SelectionCaret*>(caret_.Get()))
        {
            // The caret follows the text when it scrolls.
            if (TextBase* t = textComponent())
                if (engine::RectTransform* cr = c->rectTransform(); cr && t->rectTransform())
                    if (cr->anchoredPosition() != t->rectTransform()->anchoredPosition())
                        cr->SetAnchoredPosition(t->rectTransform()->anchoredPosition());
            c->SetVerticesDirty();
        }
    }

    i32 TextInput::CaretCharacter(i32 position) const
    {
        TextBase* t = textComponent();
        TextInfo* info = t ? t->textInfo() : nullptr;
        if (!info)
            return 0;
        for (i32 i = 0; i < info->characterCount; i++)
            if (info->characterInfo[usize(i)].index >= position)
                return i;
        return info->characterCount;
    }

    void TextInput::CaretQuads(Array<Rect>& selection, Rect& caret, bool& caretShown)
    {
        selection.Clear();
        caret = Rect(0, 0, 0, 0);
        caretShown = false;
        TextBase* t = textComponent();
        if (!t)
            return;
        TextInfo* info = t->textInfo();
        auto lineBox = [&](i32 line, f32& top, f32& bottom) {
            if (line >= 0 && line < info->lineCount)
                top = info->lineInfo[usize(line)].ascender, bottom = info->lineInfo[usize(line)].descender;
        };
        auto edge = [&](i32 position, f32& x, i32& line) {
            i32 c = CaretCharacter(position);
            line = 0;
            if (info->characterCount == 0)
            {
                Rect area = t->rectTransform()->rect();
                Vector4 m = t->margin();
                x = area.x + m.x;
                return;
            }
            if (c < info->characterCount)
            {
                const CharacterInfo& ci = info->characterInfo[usize(c)];
                x = ci.origin;
                line = ci.lineNumber;
                // A new line's caret at its line's start.
                if (ci.character == '\n' && c > 0)
                {
                    const CharacterInfo& prev = info->characterInfo[usize(c - 1)];
                    x = prev.xAdvance;
                    line = prev.lineNumber;
                }
                return;
            }
            const CharacterInfo& last = info->characterInfo[usize(info->characterCount - 1)];
            x = last.xAdvance;
            line = last.lineNumber;
            if (last.character == '\n')
            {
                line = last.lineNumber + 1;
                x = info->lineCount > line ? info->lineInfo[usize(line)].lineExtents.min.x : t->rectTransform()->rect().x + t->margin().x;
            }
        };
        if (!focused_ && !keepSelection_)
            return;
        if (HasSelection())
        {
            i32 a = CaretCharacter(focus_ < anchor_ ? focus_ : anchor_), b = CaretCharacter(focus_ > anchor_ ? focus_ : anchor_);
            i32 currentLine = -1;
            f32 x0 = 0, x1 = 0, top = 0, bottom = 0;
            for (i32 i = a; i < b && i < info->characterCount; i++)
            {
                const CharacterInfo& ci = info->characterInfo[usize(i)];
                if (ci.lineNumber != currentLine)
                {
                    if (currentLine >= 0)
                        selection.Push(Rect(x0, bottom, x1 - x0, top - bottom));
                    currentLine = ci.lineNumber;
                    x0 = ci.origin;
                    lineBox(currentLine, top, bottom);
                }
                x1 = ci.xAdvance;
            }
            if (currentLine >= 0)
                selection.Push(Rect(x0, bottom, x1 - x0, top - bottom));
            return;
        }
        if (!focused_)
            return;
        f32 x = 0;
        i32 line = 0;
        edge(focus_, x, line);
        f32 top = 0, bottom = 0;
        if (info->lineCount > 0)
            lineBox(line < info->lineCount ? line : info->lineCount - 1, top, bottom);
        else
        {
            Rect area = t->rectTransform()->rect();
            f32 size = t->fontSize();
            top = area.yMax() - t->margin().y;
            bottom = top - size * 1.2f;
        }
        if (line >= info->lineCount && info->lineCount > 0)
        {
            f32 h = top - bottom;
            top -= h, bottom -= h;
        }
        f32 w = f32(caretWidth_ > 0 ? caretWidth_ : 1);
        caret = Rect(x, bottom, w, top - bottom);
        f64 period = caretBlinkRate_ > 0 ? 1.0 / caretBlinkRate_ : 0;
        caretShown = period <= 0 || std::fmod(Now() - blinkStart_, period) < period * 0.5;
    }

    void TextInput::ScrollToCaret()
    {
        TextBase* t = textComponent();
        engine::RectTransform* viewport = textViewport();
        if (!t || !viewport)
            return;
        Array<Rect> selection;
        Rect caret;
        bool shown;
        bool wasFocused = focused_;
        focused_ = true;
        CaretQuads(selection, caret, shown);
        focused_ = wasFocused;
        engine::RectTransform* tr = t->rectTransform();
        Vector3 world = tr->TransformPoint(Vector3(caret.x, caret.y + caret.height * 0.5f, 0));
        Vector3 local = viewport->InverseTransformPoint(world);
        Rect view = viewport->rect();
        Vector2 offset(0, 0);
        if (local.x > view.xMax())
            offset.x = view.xMax() - local.x;
        else if (local.x < view.xMin())
            offset.x = view.xMin() - local.x;
        if (multiLine())
        {
            f32 half = caret.height * 0.5f;
            if (local.y - half < view.yMin())
                offset.y = view.yMin() - (local.y - half);
            else if (local.y + half > view.yMax())
                offset.y = view.yMax() - (local.y + half);
        }
        if (offset.x != 0 || offset.y != 0)
            tr->SetAnchoredPosition(Vector2(tr->anchoredPosition().x + offset.x, tr->anchoredPosition().y + offset.y));
        MarkCaret();
    }

    void TextInput::SetCaretPosition(i32 value)
    {
        SetSelection(value, value);
    }

    void TextInput::SetSelectionAnchorPosition(i32 value) { SetSelection(value, focus_); }
    void TextInput::SetSelectionFocusPosition(i32 value) { SetSelection(anchor_, value); }

    void TextInput::Rebuild(CanvasUpdate update)
    {
        if (update == CanvasUpdate::LatePreRender)
            MarkCaret();
    }

    // ------------------------------------------------------------------------------------------------ events

    namespace
    {
        // The string position under the pointer.
        i32 PositionAt(TextInput& in, PointerEventData* e)
        {
            TextBase* t = in.textComponent();
            if (!t)
                return 0;
            i32 index = TextUtilities::GetCursorIndexFromPosition(t, Vector3(e->position().x, e->position().y, 0), e->pressEventCamera());
            TextInfo* info = t->textInfo();
            if (index >= info->characterCount)
                return i32(in.Characters().Size());
            return info->characterInfo[usize(index)].index;
        }
    }

    void TextInput::OnPointerDown(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left)
            return;
        Selectable::OnPointerDown(eventData);
        if (!IsActive() || !IsInteractable())
            return;
        if (!focused_)
        {
            ActivateInputField();
            return;
        }
        i32 at = PositionAt(*this, eventData);
        bool shift = false;
        for (const engine::InputEvent& e : engine::InputFeed::Frame())
            if (e.event.type == platform::EventType::MouseDown)
                shift = (e.event.modifiers & platform::ModShift) != 0;
        if (eventData->clickCount() == 2)
            SetSelection(WordStart(at), WordEnd(at));
        else if (eventData->clickCount() >= 3)
            SetSelection(0, i32(chars_.Size()));
        else
            SetSelection(shift ? anchor_ : at, at);
    }

    void TextInput::OnBeginDrag(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left || !focused_)
            return;
        dragging_ = true;
    }

    void TextInput::OnDrag(PointerEventData* eventData)
    {
        if (!dragging_ || eventData->button() != PointerEventData::InputButton::Left)
            return;
        SetSelection(anchor_, PositionAt(*this, eventData));
    }

    void TextInput::OnEndDrag(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left)
            return;
        dragging_ = false;
        if (HasSelection())
            onEndTextSelection_.Invoke(text_, focus_ < anchor_ ? focus_ : anchor_, focus_ > anchor_ ? focus_ : anchor_);
    }

    void TextInput::OnPointerClick(PointerEventData* eventData)
    {
        if (eventData->button() != PointerEventData::InputButton::Left)
            return;
        ActivateInputField();
    }

    void TextInput::OnUpdateSelected(BaseEventData* eventData)
    {
        if (!focused_)
            return;
        // The frame's keys, characters and composition (once a frame).
        u64 frame = engine::InputFeed::FrameNumber();
        if (frame != inputFrame_)
        {
            inputFrame_ = frame;
            bool composed = false;
            for (const engine::InputEvent& e : engine::InputFeed::Frame())
            {
                if (!focused_)
                    break;
                switch (e.event.type)
                {
                case platform::EventType::KeyDown:
                    if (ProcessKey(e.event.key.key, e.event.modifiers))
                        eventData->Use();
                    break;
                case platform::EventType::Text: ProcessCharacter(e.event.text.codepoint); break;
                case platform::EventType::Composition: composed = true; break;
                default: break;
                }
            }
            if (composed)
                UpdateLabel();
        }
        eventData->Use();
    }

    void TextInput::OnScroll(PointerEventData* eventData)
    {
        TextBase* t = textComponent();
        if (!multiLine() || !t)
            return;
        engine::RectTransform* tr = t->rectTransform();
        tr->SetAnchoredPosition(Vector2(tr->anchoredPosition().x, tr->anchoredPosition().y - eventData->scrollDelta().y * scrollSensitivity_));
        MarkCaret();
    }

    void TextInput::OnSubmit(BaseEventData* eventData)
    {
        (void)eventData;
        if (!IsActive() || !IsInteractable())
            return;
        if (!focused_)
            activateNext_ = true;
    }

    void TextInput::OnSelect(BaseEventData* eventData)
    {
        Selectable::OnSelect(eventData);
        onSelect_.Invoke(text_);
        if (activateOnSelect_)
            ActivateInputField();
    }

    void TextInput::OnDeselect(BaseEventData* eventData)
    {
        DeactivateInputField();
        onDeselect_.Invoke(text_);
        Selectable::OnDeselect(eventData);
    }

    void TextInput::OnMove(AxisEventData* eventData)
    {
        if (focused_)
            return;
        Selectable::OnMove(eventData);
    }

    bool TextInput::HandlesMore(EventKind kind) const
    {
        switch (kind)
        {
        case EventKind::UpdateSelected:
        case EventKind::BeginDrag:
        case EventKind::Drag:
        case EventKind::EndDrag:
        case EventKind::PointerClick:
        case EventKind::Submit:
        case EventKind::Scroll: return true;
        default: return false;
        }
    }

    void TextInput::HandleMore(EventKind kind, BaseEventData* data)
    {
        auto* p = static_cast<PointerEventData*>(data);
        switch (kind)
        {
        case EventKind::UpdateSelected: OnUpdateSelected(data); break;
        case EventKind::BeginDrag: OnBeginDrag(p); break;
        case EventKind::Drag: OnDrag(p); break;
        case EventKind::EndDrag: OnEndDrag(p); break;
        case EventKind::PointerClick: OnPointerClick(p); break;
        case EventKind::Submit: OnSubmit(data); break;
        case EventKind::Scroll: OnScroll(p); break;
        default: break;
        }
    }

    f32 TextInput::preferredWidth() const
    {
        TextBase* t = textComponent();
        if (!t)
            return 0;
        auto* own = static_cast<engine::RectTransform*>(engine::Component::transform());
        engine::RectTransform* view = textViewport();
        f32 inset = own && view ? own->rect().width - view->rect().width : 0;
        return t->preferredWidth() + inset;
    }

    f32 TextInput::preferredHeight() const
    {
        TextBase* t = textComponent();
        if (!t)
            return 0;
        auto* own = static_cast<engine::RectTransform*>(engine::Component::transform());
        engine::RectTransform* view = textViewport();
        f32 inset = own && view ? own->rect().height - view->rect().height : 0;
        return t->preferredHeight() + inset;
    }

    // ------------------------------------------------------------------------------------------------ ScrollbarEventHandler

    void ScrollbarEventHandler::HandleEvent(EventKind kind, BaseEventData* data)
    {
        if (kind == EventKind::PointerClick)
            OnPointerClick(static_cast<PointerEventData*>(data));
        else if (kind == EventKind::Select)
            OnSelect(data);
        else if (kind == EventKind::Deselect)
            OnDeselect(data);
    }

    void TextInput::OnAfterCloneNative()
    {
        Selectable::OnAfterCloneNative();
        // The caret is found again (the copy of the original's) or made; the editing starts over.
        caret_ = engine::PPtr<engine::Object>();
        focused_ = false;
        activateNext_ = false;
        dragging_ = false;
    }
}
