#include <Helix/CanvasUI/Text/TextBase.h>

#include <Helix/CanvasUI/Text/TextMaterials.h>
#include <Helix/Engine/Asset/Resources.h>
#include <Helix/UI/Engine/UIAssets.h>
#include <Helix/Engine/Canvas/CanvasRenderer.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Engine/Transform/RectTransform.h>

#include <cmath>
#include <cstdio>
#include <limits>

namespace helix::canvasui
{
    namespace
    {
        // The reference's simple hash of link ids (TMP_TextUtilities.GetSimpleHashCode).
        i32 SimpleHash(StringView s)
        {
            i32 hash = 0;
            for (char c : s)
                hash = ((hash << 5) + hash) ^ i32(u8(c));
            return hash;
        }

        void AppendUtf8(String& s, u32 c)
        {
            if (c < 0x80)
                s += char(c);
            else if (c < 0x800)
                s += char(0xC0 | (c >> 6)), s += char(0x80 | (c & 0x3F));
            else if (c < 0x10000)
                s += char(0xE0 | (c >> 12)), s += char(0x80 | ((c >> 6) & 0x3F)), s += char(0x80 | (c & 0x3F));
            else
                s += char(0xF0 | (c >> 18)), s += char(0x80 | ((c >> 12) & 0x3F)), s += char(0x80 | ((c >> 6) & 0x3F)), s += char(0x80 | (c & 0x3F));
        }

        // Escapes typed as text: \n \t \r and \uXXXX \UXXXXXXXX.
        String ParseControlCharacters(StringView s)
        {
            String out;
            auto hex = [](char c) -> i32 {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'a' && c <= 'f')
                    return c - 'a' + 10;
                if (c >= 'A' && c <= 'F')
                    return c - 'A' + 10;
                return -1;
            };
            for (usize i = 0; i < s.Size(); i++)
            {
                char c = s[i];
                if (c != '\\' || i + 1 >= s.Size())
                {
                    out += c;
                    continue;
                }
                char n = s[i + 1];
                if (n == 'n' || n == 't' || n == 'r')
                {
                    out += n == 'n' ? '\n' : n == 't' ? '\t' : '\r';
                    i++;
                    continue;
                }
                usize digits = n == 'u' ? 4 : n == 'U' ? 8 : 0;
                // The digits are s[i + 2] to s[i + 1 + digits].
                if (digits && i + 1 + digits < s.Size())
                {
                    u32 v = 0;
                    bool ok = true;
                    for (usize k = 0; ok && k < digits; k++)
                    {
                        i32 h = hex(s[i + 2 + k]);
                        ok = h >= 0;
                        v = v * 16 + u32(h < 0 ? 0 : h);
                    }
                    if (ok)
                    {
                        AppendUtf8(out, v);
                        i += 1 + digits;
                        continue;
                    }
                }
                out += c;
            }
            return out;
        }

        // The UTF-16 index of each byte of a UTF-8 text (the reference's indices are the string's).
        Array<i32> Utf16Indices(StringView s)
        {
            Array<i32> out;
            out.Resize(s.Size() + 1);
            i32 u = 0;
            for (usize i = 0; i < s.Size();)
            {
                u8 b = u8(s[i]);
                usize len = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 1;
                for (usize k = 0; k < len && i + k < s.Size(); k++)
                    out[i + k] = u;
                u += len == 4 ? 2 : 1;
                i += len;
            }
            out[s.Size()] = u;
            return out;
        }

        u32 StyleFlags(FontStyles s)
        {
            u32 v = u32(s), f = 0;
            if (v & u32(FontStyles::Bold))
                f |= ui::TextFlags::Bold;
            if (v & u32(FontStyles::Italic))
                f |= ui::TextFlags::Italic;
            if (v & u32(FontStyles::Underline))
                f |= ui::TextFlags::Underline;
            if (v & u32(FontStyles::LowerCase))
                f |= ui::TextFlags::Lowercase;
            if (v & u32(FontStyles::UpperCase))
                f |= ui::TextFlags::Uppercase;
            if (v & u32(FontStyles::SmallCaps))
                f |= ui::TextFlags::SmallCaps;
            if (v & u32(FontStyles::Strikethrough))
                f |= ui::TextFlags::Strikethrough;
            if (v & u32(FontStyles::Superscript))
                f |= ui::TextFlags::Superscript;
            if (v & u32(FontStyles::Subscript))
                f |= ui::TextFlags::Subscript;
            if (v & u32(FontStyles::Highlight))
                f |= ui::TextFlags::Mark;
            return f;
        }

        FontStyles StylesOf(u32 f)
        {
            i32 s = 0;
            s |= (f & ui::TextFlags::Bold) ? i32(FontStyles::Bold) : 0;
            s |= (f & ui::TextFlags::Italic) ? i32(FontStyles::Italic) : 0;
            s |= (f & ui::TextFlags::Underline) ? i32(FontStyles::Underline) : 0;
            s |= (f & ui::TextFlags::Lowercase) ? i32(FontStyles::LowerCase) : 0;
            s |= (f & ui::TextFlags::Uppercase) ? i32(FontStyles::UpperCase) : 0;
            s |= (f & ui::TextFlags::SmallCaps) ? i32(FontStyles::SmallCaps) : 0;
            s |= (f & ui::TextFlags::Strikethrough) ? i32(FontStyles::Strikethrough) : 0;
            s |= (f & ui::TextFlags::Superscript) ? i32(FontStyles::Superscript) : 0;
            s |= (f & ui::TextFlags::Subscript) ? i32(FontStyles::Subscript) : 0;
            s |= (f & ui::TextFlags::Mark) ? i32(FontStyles::Highlight) : 0;
            return FontStyles(s);
        }

        HorizontalAlignmentOptions AlignmentOf(ui::TextAlignment a)
        {
            switch (a)
            {
            case ui::TextAlignment::Center: return HorizontalAlignmentOptions::Center;
            case ui::TextAlignment::Right: return HorizontalAlignmentOptions::Right;
            case ui::TextAlignment::Justified: return HorizontalAlignmentOptions::Justified;
            case ui::TextAlignment::Flush: return HorizontalAlignmentOptions::Flush;
            default: return HorizontalAlignmentOptions::Left;
            }
        }

        bool IsSpace(char32_t c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0xA0 || c == 0x3000 || c == 0x200B; }

        Color32 ToColor32(Color c)
        {
            auto b = [](f32 v) { return u8(v <= 0 ? 0 : v >= 1 ? 255 : i32(v * 255.0f + 0.5f)); };
            return Color32(b(c.r), b(c.g), b(c.b), b(c.a));
        }

        Color32 Multiply(Color32 a, Color b)
        {
            return ToColor32(Color(f32(a.r) / 255.0f * b.r, f32(a.g) / 255.0f * b.g, f32(a.b) / 255.0f * b.b, f32(a.a) / 255.0f * b.a));
        }
    }

    // ------------------------------------------------------------------------------------------------ the tags' assets

    namespace
    {
        struct Requests
        {
            String name;
            engine::PPtr<engine::Object> font, sprites;
        };

        Requests& Asked()
        {
            static Requests* r = new Requests();
            return *r;
        }

        // A sprite of a <sprite> tag: its asset (the tag's, the text's, the settings'), then the sprite by index,
        // name or code point (with the asset's fallbacks).
        bool FindSprite(const ui::RichTextElement& e, f32 size, ui::TextSprite& out, void* user)
        {
            auto* text = static_cast<const TextBase*>(user);
            SpriteAsset* asset = !e.spriteAsset.Empty() ? TextBase::FindSpriteAsset(e.spriteAsset) : nullptr;
            if (!asset && e.spriteAsset.Empty())
                asset = text->spriteAsset() ? text->spriteAsset() : TextSettings::defaultSpriteAsset();
            if (!asset)
                return false;
            i32 index = -1;
            SpriteAsset* holder = asset;
            if (e.spriteIndex >= 0)
                index = e.spriteIndex;
            else if (!e.spriteName.Empty())
                holder = SpriteAsset::SearchForSpriteByHashCode(asset, MaterialReferenceManager::HashOf(e.spriteName), true, index);
            else
                holder = SpriteAsset::SearchForSpriteByUnicode(asset, u32(e.code), true, index);
            if (!holder || index < 0 || !holder->Resolve(index, size, out))
                return false;
            out.tint = text->tintAllSprites();
            return true;
        }

        ui::TextFont* FindFont(StringView name, void* user)
        {
            (void)user;
            ui::FontAsset* f = TextBase::FindFontAsset(name);
            return f ? f->textFont() : nullptr;
        }
    }

    engine::Event<TextBase*>& TextBase::textChanged()
    {
        static engine::Event<TextBase*>* e = new engine::Event<TextBase*>();
        return *e;
    }

    engine::Event<i32>& TextBase::fontAssetRequest()
    {
        static engine::Event<i32>* e = new engine::Event<i32>();
        return *e;
    }

    engine::Event<i32>& TextBase::spriteAssetRequest()
    {
        static engine::Event<i32>* e = new engine::Event<i32>();
        return *e;
    }

    String TextBase::RequestNameInternal() { return Asked().name; }
    void TextBase::SetRequestedFontInternal(ui::FontAsset* font) { Asked().font = engine::PPtr<engine::Object>(font); }
    void TextBase::SetRequestedSpriteInternal(SpriteAsset* sprites) { Asked().sprites = engine::PPtr<engine::Object>(sprites); }

    ui::FontAsset* TextBase::FindFontAsset(StringView name)
    {
        i32 hash = MaterialReferenceManager::HashOf(name);
        ui::FontAsset* f = nullptr;
        if (MaterialReferenceManager::TryGetFontAsset(hash, f))
            return f;
        String path(TextSettings::defaultFontAssetPath());
        path.Append(name);
        f = ui::ScriptObjectAs<ui::FontAsset>(engine::Resources::Load(path));
        if (!f)
        {
            Requests& r = Asked();
            r.name = String(name);
            r.font = engine::PPtr<engine::Object>();
            fontAssetRequest().Invoke(hash);
            f = ui::ScriptObjectAs<ui::FontAsset>(r.font.Get());
        }
        if (f)
            MaterialReferenceManager::AddFontAsset(f);
        return f;
    }

    SpriteAsset* TextBase::FindSpriteAsset(StringView name)
    {
        i32 hash = MaterialReferenceManager::HashOf(name);
        SpriteAsset* s = nullptr;
        if (MaterialReferenceManager::TryGetSpriteAsset(hash, s))
            return s;
        String path(TextSettings::defaultSpriteAssetPath());
        path.Append(name);
        engine::Object* o = engine::Resources::Load(path);
        s = o && o->Is<SpriteAsset>() ? static_cast<SpriteAsset*>(o) : nullptr;
        if (!s)
        {
            Requests& r = Asked();
            r.name = String(name);
            r.sprites = engine::PPtr<engine::Object>();
            spriteAssetRequest().Invoke(hash);
            s = static_cast<SpriteAsset*>(r.sprites.Get());
        }
        if (s)
            MaterialReferenceManager::AddSpriteAsset(hash, s);
        return s;
    }

    bool TextBase::Sheets::Find(StringView name, String& opening, String& closing) const
    {
        if (TextStyleSheet* own = text ? text->styleSheet() : nullptr)
            if (own->Find(name, opening, closing))
                return true;
        TextStyleSheet* shared = TextSettings::defaultStyleSheet();
        return shared && shared->Find(name, opening, closing);
    }

    void TextBase::SetSpriteAsset(SpriteAsset* value)
    {
        spriteAsset_ = engine::PPtr<engine::Object>(value);
        changed_ = true;
        SetVerticesDirty();
        SetLayoutDirty();
    }

    void TextBase::SetStyleSheet(TextStyleSheet* value)
    {
        styleSheet_ = engine::PPtr<engine::Object>(value);
        changed_ = true;
        SetVerticesDirty();
        SetLayoutDirty();
    }

    void TextBase::SetColorGradientPreset(TextColorGradient* value)
    {
        gradientPreset_ = engine::PPtr<engine::Object>(value);
        changed_ = true;
        SetVerticesDirty();
    }

    TextBase::TextBase()
    {
        sheets_.text = this;
        info_ = new TextInfo(this);
        mesh_ = engine::Mesh::Create();
        mesh_->SetName("Text Mesh");
    }

    TextBase::~TextBase()
    {
        delete info_;
        info_ = nullptr;
        if (mesh_)
            engine::Object::DestroyImmediate(mesh_);
        for (engine::Mesh* m : subMeshes_)
            if (m)
                engine::Object::DestroyImmediate(m);
    }

    // ------------------------------------------------------------------------------------------------ the text

    void TextBase::SetText(StringView value)
    {
        if (text_ == value)
            return;
        text_ = String(value);
        changed_ = true;
        SetVerticesDirty();
        SetLayoutDirty();
    }

    void TextBase::SetTextFormat(StringView sourceText, f32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7)
    {
        const f32 args[8] = {arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7};
        String out;
        for (usize i = 0; i < sourceText.Size(); i++)
        {
            char c = sourceText[i];
            // {n} or {n:d}: argument n with d decimals (none: rounded to a whole number).
            if (c == '{' && i + 2 < sourceText.Size() && sourceText[i + 1] >= '0' && sourceText[i + 1] <= '7')
            {
                usize j = i + 2;
                i32 decimals = 0;
                if (j < sourceText.Size() && sourceText[j] == ':')
                {
                    j++;
                    while (j < sourceText.Size() && sourceText[j] >= '0' && sourceText[j] <= '9')
                        decimals = decimals * 10 + (sourceText[j++] - '0');
                }
                if (j < sourceText.Size() && sourceText[j] == '}')
                {
                    char buffer[64];
                    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals > 9 ? 9 : decimals, double(args[sourceText[i + 1] - '0']));
                    out.Append(StringView(buffer));
                    i = j;
                    continue;
                }
            }
            out += c;
        }
        SetText(out);
    }

    void TextBase::SetCharArray(Span<const char16_t> sourceText, i32 start, i32 length)
    {
        String out;
        for (i32 i = start; i < start + length && i >= 0 && usize(i) < sourceText.Size(); i++)
        {
            u32 c = sourceText[usize(i)];
            if (c >= 0xD800 && c < 0xDC00 && usize(i + 1) < sourceText.Size())
            {
                u32 lo = sourceText[usize(i + 1)];
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                i++;
            }
            AppendUtf8(out, c);
        }
        SetText(out);
    }

    String TextBase::SourceText() const
    {
        String body = parseCtrlCharacters_ ? ParseControlCharacters(text_) : text_;
        // The style of the whole text around it (the default style's hash: none).
        if (textStyleHash_ != -1183493901 && textStyleHash_ != 0)
        {
            TextStyleSheet* sheet = styleSheet() ? styleSheet() : TextSettings::defaultStyleSheet();
            i32 i = sheet ? sheet->FindStyleInternal(textStyleHash_) : -1;
            if (i < 0 && styleSheet() && TextSettings::defaultStyleSheet())
            {
                sheet = TextSettings::defaultStyleSheet();
                i = sheet->FindStyleInternal(textStyleHash_);
            }
            if (i >= 0)
            {
                String wrapped = sheet->StyleOpeningInternal(i);
                wrapped.Append(body);
                wrapped.Append(sheet->StyleClosingInternal(i));
                return wrapped;
            }
        }
        return body;
    }

    // ------------------------------------------------------------------------------------------------ font and materials

    ui::FontAsset* TextBase::font() const
    {
        // Without one the settings' default font.
        if (auto* f = ui::ScriptObjectAs<ui::FontAsset>(font_.Get()))
            return f;
        return TextSettings::defaultFontAsset();
    }

    void TextBase::SetFont(ui::FontAsset* value)
    {
        if (font() == value)
            return;
        font_ = engine::PPtr<engine::Object>(value);
        changed_ = true;
        SetAllDirty();
    }

    engine::Material* TextBase::fontSharedMaterial() const
    {
        if (engine::Material* m = sharedMaterial_.Get())
            return m;
        return TextMaterials::FontMaterial(font());
    }

    void TextBase::SetFontSharedMaterial(engine::Material* value)
    {
        sharedMaterial_ = engine::PPtr<engine::Material>(value);
        instanceMaterial_ = engine::PPtr<engine::Material>();
        changed_ = true;
        SetMaterialDirty();
        SetVerticesDirty();
    }

    engine::Material* TextBase::fontMaterial()
    {
        if (engine::Material* m = instanceMaterial_.Get())
            return m;
        engine::Material* shared = fontSharedMaterial();
        if (!shared)
            return nullptr;
        engine::Material* copy = engine::Material::Create(*shared);
        copy->SetName(Format("{} (Instance)", shared->Name()));
        instanceMaterial_ = engine::PPtr<engine::Material>(copy);
        sharedMaterial_ = engine::PPtr<engine::Material>(copy);
        SetMaterialDirty();
        SetVerticesDirty();
        return copy;
    }

    void TextBase::SetFontMaterial(engine::Material* value) { SetFontSharedMaterial(value); }

    Array<engine::Material*> TextBase::fontSharedMaterials() const
    {
        Array<engine::Material*> out;
        for (const MeshInfo& m : info_->meshInfo)
            out.Push(m.material);
        if (out.Empty())
            out.Push(fontSharedMaterial());
        return out;
    }

    Array<engine::Material*> TextBase::fontMaterials()
    {
        Array<engine::Material*> out = fontSharedMaterials();
        if (!out.Empty())
            out[0] = fontMaterial();
        return out;
    }

    Color32 TextBase::faceColor()
    {
        engine::Material* m = fontSharedMaterial();
        return m ? ToColor32(m->GetColor("_FaceColor")) : Color32(255, 255, 255, 255);
    }

    void TextBase::SetFaceColor(Color32 value)
    {
        if (engine::Material* m = fontMaterial())
            m->SetColor("_FaceColor", Color(f32(value.r) / 255.0f, f32(value.g) / 255.0f, f32(value.b) / 255.0f, f32(value.a) / 255.0f));
        SetMaterialDirty();
    }

    Color32 TextBase::outlineColor()
    {
        engine::Material* m = fontSharedMaterial();
        return m ? ToColor32(m->GetColor("_OutlineColor")) : Color32(0, 0, 0, 255);
    }

    void TextBase::SetOutlineColor(Color32 value)
    {
        if (engine::Material* m = fontMaterial())
            m->SetColor("_OutlineColor", Color(f32(value.r) / 255.0f, f32(value.g) / 255.0f, f32(value.b) / 255.0f, f32(value.a) / 255.0f));
        SetMaterialDirty();
    }

    f32 TextBase::outlineWidth()
    {
        engine::Material* m = fontSharedMaterial();
        return m ? m->GetFloat("_OutlineWidth") : 0.0f;
    }

    void TextBase::SetOutlineWidth(f32 value)
    {
        if (engine::Material* m = fontMaterial())
            m->SetFloat("_OutlineWidth", value);
        SetMaterialDirty();
        SetVerticesDirty();
    }

    // ------------------------------------------------------------------------------------------------ size, color, layout

    void TextBase::SetFontSize(f32 value)
    {
        fontSizeBase_ = value;
        Change(fontSize_, value);
    }

    void TextBase::SetColor(Color value)
    {
        if (color() == value)
            return;
        MaskableGraphic::SetColor(value);
        changed_ = true;
        SetVerticesDirty();
    }

    void TextBase::SetAlpha(f32 value)
    {
        Color c = color();
        c.a = value;
        SetColor(c);
    }

    void TextBase::SetColorGradient(VertexGradient value)
    {
        colorGradient_ = value;
        changed_ = true;
        SetVerticesDirty();
    }

    TextBase* TextBase::linkedTextComponent() const { return static_cast<TextBase*>(linked_.Get()); }

    void TextBase::SetLinkedTextComponent(TextBase* value)
    {
        if (value == this)
            return;
        linked_ = engine::PPtr<engine::Object>(value);
        changed_ = true;
        SetVerticesDirty();
    }

    void TextBase::SetAutoSizeTextContainer(bool value)
    {
        autoSizeTextContainer_ = value;
        if (value)
            if (engine::RectTransform* r = rectTransform())
                r->SetSizeDelta(GetPreferredValues(std::numeric_limits<f32>::infinity(), std::numeric_limits<f32>::infinity()));
    }

    void TextBase::SetHavePropertiesChanged(bool value)
    {
        changed_ = value;
        if (value)
        {
            SetVerticesDirty();
            SetLayoutDirty();
        }
    }

    // ------------------------------------------------------------------------------------------------ dirtiness

    void TextBase::SetVerticesDirty()
    {
        changed_ = true;
        MaskableGraphic::SetVerticesDirty();
    }

    void TextBase::SetLayoutDirty()
    {
        if (!IsActive())
            return;
        if (engine::RectTransform* r = rectTransform())
            LayoutRebuilder::MarkLayoutForRebuild(r);
    }

    void TextBase::OnEnable()
    {
        MaskableGraphic::OnEnable();
        changed_ = true;
    }

    void TextBase::OnDisable() { MaskableGraphic::OnDisable(); }

    void TextBase::OnDestroy()
    {
        if (engine::Material* m = instanceMaterial_.Get())
            engine::Object::DestroyImmediate(m);
        MaskableGraphic::OnDestroy();
    }

    void TextBase::OnRectTransformDimensionsChange()
    {
        MaskableGraphic::OnRectTransformDimensionsChange();
        changed_ = true;
    }

    // ------------------------------------------------------------------------------------------------ settings

    Rect TextBase::TextArea() const
    {
        engine::RectTransform* r = rectTransform();
        Rect rect = r ? r->rect() : Rect(0, 0, 0, 0);
        Vector4 m = margin_;
        return Rect(rect.x + m.x, rect.y + m.w, rect.width - m.x - m.z, rect.height - m.y - m.w);
    }

    f32 TextBase::maxWidth() const { return TextArea().width; }
    f32 TextBase::maxHeight() const { return TextArea().height; }

    f32 TextBase::pixelsPerUnit() const { return PixelScale(); }

    ui::TextGenerationSettings TextBase::Settings(f32 width, f32 height) const
    {
        ui::TextGenerationSettings s;
        ui::FontAsset* f = font();
        s.font = f ? f->textFont() : nullptr;
        // The settings' fallback fonts after the font's own; the tags' fonts, sprites and styles.
        static thread_local Array<ui::TextFont*> fallbacks;
        fallbacks.Clear();
        for (ui::FontAsset* fb : TextSettings::FallbackFontAssetsInternal())
            if (ui::TextFont* t = fb ? fb->textFont() : nullptr)
                fallbacks.Push(t);
        s.fallbacks = Span<ui::TextFont* const>(fallbacks.Data(), fallbacks.Size());
        s.findFont = &FindFont;
        s.findSprite = &FindSprite;
        s.user = const_cast<TextBase*>(this);
        s.styleSheet = &sheets_;
        // Points in the text's units: a world text's are a tenth of a unit (FontScale).
        s.fontSize = fontSize_ * FontScale();
        s.color = color();
        s.fontStyle = StyleFlags(fontStyle_);
        if (i32(fontWeight_) >= i32(FontWeight::SemiBold))
            s.fontStyle |= ui::TextFlags::Bold;
        switch (horizontalAlignment())
        {
        case HorizontalAlignmentOptions::Center:
        case HorizontalAlignmentOptions::Geometry: s.alignment = ui::TextAlignment::Center; break;
        case HorizontalAlignmentOptions::Right: s.alignment = ui::TextAlignment::Right; break;
        case HorizontalAlignmentOptions::Justified: s.alignment = ui::TextAlignment::Justified; break;
        case HorizontalAlignmentOptions::Flush: s.alignment = ui::TextAlignment::Flush; break;
        default: s.alignment = ui::TextAlignment::Left; break;
        }
        switch (verticalAlignment())
        {
        case VerticalAlignmentOptions::Middle: s.verticalAlignment = ui::TextVerticalAlignment::Middle; break;
        case VerticalAlignmentOptions::Bottom: s.verticalAlignment = ui::TextVerticalAlignment::Bottom; break;
        case VerticalAlignmentOptions::Baseline: s.verticalAlignment = ui::TextVerticalAlignment::Baseline; break;
        case VerticalAlignmentOptions::Geometry: s.verticalAlignment = ui::TextVerticalAlignment::Geometry; break;
        case VerticalAlignmentOptions::Capline: s.verticalAlignment = ui::TextVerticalAlignment::Capline; break;
        default: s.verticalAlignment = ui::TextVerticalAlignment::Top; break;
        }
        s.characterSpacing = characterSpacing_;
        s.wordSpacing = wordSpacing_;
        s.lineSpacing = lineSpacing_;
        s.paragraphSpacing = paragraphSpacing_;
        s.wrapping = ui::TextWrappingMode(i32(wrapping_));
        switch (overflow_)
        {
        case TextOverflowModes::Ellipsis: s.overflow = ui::TextOverflowMode::Ellipsis; break;
        case TextOverflowModes::Masking: s.overflow = ui::TextOverflowMode::Masking; break;
        case TextOverflowModes::Truncate:
        case TextOverflowModes::Linked: s.overflow = ui::TextOverflowMode::Truncate; break;
        case TextOverflowModes::Page: s.overflow = ui::TextOverflowMode::Page; break;
        default: s.overflow = ui::TextOverflowMode::Overflow; break;
        }
        s.direction = isRightToLeft_ ? ui::BidiDirection::RightToLeft : ui::BidiDirection::Auto;
        s.richText = richText_;
        s.width = width;
        s.height = height;
        s.autoSize = autoSize_;
        s.minFontSize = fontSizeMin_ * FontScale();
        s.maxFontSize = fontSizeMax_ * FontScale();
        s.page = pageToDisplay_ > 0 ? u32(pageToDisplay_ - 1) : 0;
        return s;
    }

    // ------------------------------------------------------------------------------------------------ generation

    void TextBase::UpdateFontAtlases()
    {
        if (ui::FontAsset* f = font())
            f->UpdateAtlasTextures();
        for (const ui::TextQuad& q : layout_.quads)
            if (q.font)
                if (ui::FontAsset* other = ui::FontAsset::Of(q.font); other && other != font())
                    other->UpdateAtlasTextures();
    }

    void TextBase::ForceMeshUpdate(bool ignoreActiveState, bool forceTextReparsing)
    {
        (void)forceTextReparsing;
        if (!ignoreActiveState && !IsActive())
            return;
        changed_ = true;
        GenerateText();
    }

    void TextBase::GenerateText()
    {
        if (generating_)
            return;
        generating_ = true;
        changed_ = false;
        Rect area = TextArea();
        info_->Clear();
        layout_.Clear();
        firstOverflow_ = -1;
        overflowing_ = truncated_ = usingBold_ = false;
        ui::TextGenerationSettings settings = Settings(area.width, area.height);
        String source = SourceText();
        if (settings.font && !source.Empty())
        {
            generator_.Generate(source, settings, layout_);
            if (autoSize_)
                fontSize_ = layout_.fontSize / FontScale();
        }
        UpdateFontAtlases();
        BuildInfo(area);
        BuildMeshes(area);
        info_->generation++;
        onPreRenderText_.Invoke(info_);
        if (renderMode_ != TextRenderFlags::DontRender)
            UploadMeshes();
        textChanged().Invoke(this);
        // Linked: the rest continues in the next component.
        if (overflow_ == TextOverflowModes::Linked)
            if (TextBase* next = linkedTextComponent(); next && next != this)
            {
                String rest;
                if (firstOverflow_ >= 0 && usize(firstOverflow_) < info_->characterInfo.Size())
                {
                    Array<i32> map = Utf16Indices(source);
                    // The byte of the first character that did not fit.
                    usize byte = 0;
                    for (usize b = 0; b < map.Size(); b++)
                        if (map[b] == info_->characterInfo[usize(firstOverflow_)].index)
                        {
                            byte = b;
                            break;
                        }
                    rest = String(StringView(source).Substr(byte));
                }
                next->SetText(rest);
                next->ForceMeshUpdate(true);
            }
        if (autoSizeTextContainer_)
            if (engine::RectTransform* r = rectTransform())
            {
                Vector2 size(layout_.width + margin_.x + margin_.z, layout_.height + margin_.y + margin_.w);
                if (r->sizeDelta() != size)
                    r->SetSizeDelta(size);
            }
        generating_ = false;
    }

    void TextBase::BuildInfo(const Rect& area)
    {
        TextInfo& info = *info_;
        String source = SourceText();
        Array<i32> map = Utf16Indices(source);
        f32 left = area.x, top = area.y + area.height;
        auto X = [&](f32 x) { return left + x; };
        auto Y = [&](f32 y) { return top - y; };
        const usize n = layout_.characterInfo.Size();
        info.characterInfo.Resize(n);
        for (usize i = 0; i < n; i++)
        {
            const ui::CharacterInfo& u = layout_.characterInfo[i];
            CharacterInfo& c = info.characterInfo[i];
            c = CharacterInfo();
            c.character = u.character;
            c.index = u.index < map.Size() ? map[u.index] : i32(u.index);
            c.stringLength = u.character > 0xFFFF ? 2 : 1;
            c.elementType = u.elementType == ui::TextElementKind::Sprite ? TextElementType::Sprite : TextElementType::Character;
            c.fontAsset = u.font ? ui::FontAsset::Of(u.font) : font();
            c.pointSize = u.pointSize;
            c.scale = fontSize_ > 0 ? u.pointSize / (fontSize_ * FontScale()) : 1;
            c.lineNumber = i32(u.lineNumber);
            c.pageNumber = i32(u.pageNumber);
            // The glyph's own box: its quad less the distance field's padding (the vertices keep it).
            f32 pad = 0;
            if (u.isVisible && u.elementType == ui::TextElementKind::Character && u.font && u.font->Cache().SamplingSize() > 0)
                pad = f32(u.font->Cache().Padding()) * u.pointSize / f32(u.font->Cache().SamplingSize());
            c.topLeft = Vector3(X(u.topLeft.x + pad), Y(u.topLeft.y + pad), 0);
            c.topRight = Vector3(X(u.topRight.x - pad), Y(u.topRight.y + pad), 0);
            c.bottomLeft = Vector3(X(u.bottomLeft.x + pad), Y(u.bottomLeft.y - pad), 0);
            c.bottomRight = Vector3(X(u.bottomRight.x - pad), Y(u.bottomRight.y - pad), 0);
            c.origin = X(u.origin);
            c.xAdvance = X(u.xAdvance);
            c.ascender = Y(u.ascender);
            c.baseLine = Y(u.baseLine);
            c.descender = Y(u.descender);
            c.isVisible = u.isVisible;
            c.color = ToColor32(overrideColorTags_ ? color() : u.color);
            c.style = StylesOf(u.style);
            c.vertexIndex = -1;
            if (u.style & ui::TextFlags::Bold)
                usingBold_ = true;
            if (IsSpace(u.character))
                info.spaceCount++;
            if (c.elementType == TextElementType::Sprite)
                info.spriteCount++;
        }
        info.characterCount = i32(n);
        for (const ui::WordInfo& w : layout_.wordInfo)
            info.wordInfo.Push(WordInfo{this, i32(w.firstCharacterIndex), i32(w.lastCharacterIndex), i32(w.characterCount)});
        info.wordCount = i32(info.wordInfo.Size());
        for (const ui::LineInfo& l : layout_.lineInfo)
        {
            LineInfo li;
            li.characterCount = i32(l.characterCount);
            li.visibleCharacterCount = i32(l.visibleCharacterCount);
            li.spaceCount = i32(l.spaceCount);
            li.wordCount = i32(l.wordCount);
            li.firstCharacterIndex = i32(l.firstCharacterIndex);
            li.lastCharacterIndex = i32(l.lastCharacterIndex);
            li.firstVisibleCharacterIndex = li.lastVisibleCharacterIndex = -1;
            for (i32 k = li.firstCharacterIndex; k <= li.lastCharacterIndex && usize(k) < n; k++)
                if (info.characterInfo[usize(k)].isVisible && !IsSpace(info.characterInfo[usize(k)].character))
                {
                    if (li.firstVisibleCharacterIndex < 0)
                        li.firstVisibleCharacterIndex = k;
                    li.lastVisibleCharacterIndex = k;
                }
                else if (IsSpace(info.characterInfo[usize(k)].character) && info.characterInfo[usize(k)].isVisible)
                    li.visibleSpaceCount++;
            li.width = area.width;
            li.length = l.width;
            li.lineHeight = l.lineHeight;
            li.ascender = Y(l.ascender);
            li.baseline = Y(l.baseline);
            li.descender = Y(l.descender);
            li.maxAdvance = l.width;
            li.alignment = AlignmentOf(l.alignment);
            li.lineExtents.min = Vector2(X(l.x), Y(l.descender));
            li.lineExtents.max = Vector2(X(l.x + l.width), Y(l.ascender));
            info.lineInfo.Push(li);
        }
        info.lineCount = i32(info.lineInfo.Size());
        for (const ui::PageInfo& p : layout_.pageInfo)
            info.pageInfo.Push(PageInfo{i32(p.firstCharacterIndex), i32(p.lastCharacterIndex), Y(p.ascender), 0, Y(p.descender)});
        info.pageCount = i32(info.pageInfo.Size());
        for (const ui::LinkInfo& l : layout_.linkInfo)
        {
            LinkInfo li;
            li.textComponent = this;
            li.linkId = l.id;
            li.hashCode = SimpleHash(l.id);
            li.linkIdLength = i32(l.id.Size());
            li.linkTextfirstCharacterIndex = i32(l.firstCharacterIndex);
            li.linkTextLength = i32(l.characterCount);
            info.linkInfo.Push(std::move(li));
        }
        info.linkCount = i32(info.linkInfo.Size());
        // Overflow: the text did not fit (Overflow and Masking draw it anyway), the first character that went.
        overflowing_ = layout_.overflowed || (area.height >= 0 && layout_.height > area.height + 0.01f);
        if (layout_.overflowed && (overflow_ == TextOverflowModes::Truncate || overflow_ == TextOverflowModes::Ellipsis || overflow_ == TextOverflowModes::Linked))
        {
            truncated_ = true;
            for (usize i = 0; i < n; i++)
                if (!info.characterInfo[i].isVisible && !IsSpace(info.characterInfo[i].character))
                {
                    firstOverflow_ = i32(i);
                    break;
                }
        }
    }

    bool TextBase::Hidden(const CharacterInfo& c, i32 index) const
    {
        if (index >= maxVisibleCharacters_ || c.lineNumber >= maxVisibleLines_)
            return true;
        if (maxVisibleWords_ < 99999)
        {
            i32 word = -1;
            for (usize w = 0; w < info_->wordInfo.Size(); w++)
                if (index >= info_->wordInfo[w].firstCharacterIndex && index <= info_->wordInfo[w].lastCharacterIndex)
                    word = i32(w);
            if (word >= maxVisibleWords_)
                return true;
        }
        return false;
    }

    engine::Material* TextBase::MaterialFor(const ui::TextQuad& quad)
    {
        if (quad.sprite || quad.colorGlyph)
            return engine::Canvas::GetDefaultCanvasMaterial();
        ui::FontAsset* f = quad.font ? ui::FontAsset::Of(quad.font) : font();
        if (f == font())
            return fontSharedMaterial();
        return TextMaterials::FontMaterial(f);
    }

    void TextBase::BuildMeshes(const Rect& area)
    {
        TextInfo& info = *info_;
        f32 left = area.x, top = area.y + area.height;
        // One mesh per (font, page, color glyphs or sprite texture): the main font's first page first.
        struct Key
        {
            const void* font;
            u32 page;
            bool color;
            const void* sprite;
            bool operator==(const Key& o) const { return font == o.font && page == o.page && color == o.color && sprite == o.sprite; }
        };
        Array<Key> keys;
        Array<Array<usize>> quadsOf;
        ui::FontAsset* main = font();
        const void* mainFont = main ? static_cast<const void*>(main->textFont()) : nullptr;
        keys.Push(Key{mainFont, 0, false, nullptr});
        quadsOf.Push(Array<usize>());
        for (usize q = 0; q < layout_.quads.Size(); q++)
        {
            const ui::TextQuad& quad = layout_.quads[q];
            if (quad.character != ~0u && quad.character < info.characterInfo.Size() && Hidden(info.characterInfo[quad.character], i32(quad.character)))
                continue;
            Key k{quad.sprite ? nullptr : static_cast<const void*>(quad.font), quad.sprite ? 0u : quad.page, quad.colorGlyph && !quad.sprite, quad.sprite};
            usize at = keys.Size();
            for (usize i = 0; i < keys.Size(); i++)
                if (keys[i] == k)
                    at = i;
            if (at == keys.Size())
            {
                keys.Push(k);
                quadsOf.Push(Array<usize>());
            }
            quadsOf[at].Push(q);
        }
        // Characters hidden by the visible counts.
        for (usize i = 0; i < info.characterInfo.Size(); i++)
            if (Hidden(info.characterInfo[i], i32(i)))
                info.characterInfo[i].isVisible = false;
        // The decorations (underlines, strikethroughs, marks): a mesh of solid quads.
        usize decorations = layout_.rects.Empty() ? ~usize(0) : keys.Size();
        info.meshInfo.Resize(keys.Size() + (layout_.rects.Empty() ? 0 : 1));
        meshTextures_.Clear();
        for (usize mi = 0; mi < info.meshInfo.Size(); mi++)
        {
            engine::Texture* texture = nullptr;
            if (mi == decorations)
                texture = engine::Texture2D::whiteTexture();
            else if (keys[mi].sprite)
                texture = static_cast<engine::Texture*>(const_cast<void*>(keys[mi].sprite));
            else if (ui::FontAsset* f = keys[mi].font ? ui::FontAsset::Of(static_cast<const ui::TextFont*>(keys[mi].font)) : main)
                texture = f->AtlasTexture(keys[mi].page, keys[mi].color);
            meshTextures_.Push(engine::PPtr<engine::Texture>(texture));
        }
        while (subMeshes_.Size() + 1 < info.meshInfo.Size())
        {
            engine::Mesh* m = engine::Mesh::Create();
            m->SetName("Text Sub Mesh");
            subMeshes_.Push(m);
        }
        f32 pixel = PixelScale();
        bool any = false;
        Vector3 bmin(std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(), 0), bmax(-std::numeric_limits<f32>::max(), -std::numeric_limits<f32>::max(), 0);
        auto grow = [&](Vector3 p) {
            any = true;
            bmin = Vector3(p.x < bmin.x ? p.x : bmin.x, p.y < bmin.y ? p.y : bmin.y, 0);
            bmax = Vector3(p.x > bmax.x ? p.x : bmax.x, p.y > bmax.y ? p.y : bmax.y, 0);
        };
        for (usize mi = 0; mi < info.meshInfo.Size(); mi++)
        {
            MeshInfo& m = info.meshInfo[mi];
            m.mesh = mi == 0 ? mesh_ : subMeshes_[mi - 1];
            bool decor = mi == decorations;
            usize count = decor ? layout_.rects.Size() : quadsOf[mi].Size();
            m.ResizeMeshInfo(i32(count));
            m.vertexCount = i32(count * 4);
            m.material = nullptr;
            for (usize k = 0; k < count; k++)
            {
                Vector2 corners[4];
                Rect uv(0, 0, 1, 1);
                Color tint;
                f32 w = 0, kind = 0;
                u32 character = ~0u;
                if (decor)
                {
                    const ui::TextRect& r = layout_.rects[k];
                    corners[0] = Vector2(r.rect.x, r.rect.y), corners[1] = Vector2(r.rect.x + r.rect.width, r.rect.y);
                    corners[2] = Vector2(r.rect.x + r.rect.width, r.rect.y + r.rect.height), corners[3] = Vector2(r.rect.x, r.rect.y + r.rect.height);
                    tint = r.color;
                }
                else
                {
                    const ui::TextQuad& q = layout_.quads[quadsOf[mi][k]];
                    for (int c = 0; c < 4; c++)
                        corners[c] = q.corners[c];
                    uv = q.uv;
                    tint = q.color;
                    character = q.character;
                    if (!q.sprite && !q.colorGlyph && q.font)
                    {
                        // Screen pixels per distance unit: the field spans twice the padding; bold flips the sign.
                        w = 2.0f * f32(q.font->Cache().Padding()) * q.sdfScale * pixel;
                        if (q.bold)
                            w = -w;
                        // The shader's kind of glyph: a distance field, a multi channel one, coverage.
                        ui::GlyphRenderMode mode = q.font->Cache().Mode();
                        kind = mode == ui::GlyphRenderMode::Sdf ? 0.0f : mode == ui::GlyphRenderMode::Msdf ? 1.0f : 2.0f;
                    }
                    if (!m.material)
                        m.material = MaterialFor(q);
                }
                if (overrideColorTags_ && !decor)
                    tint = Color(color().r, color().g, color().b, tint.a);
                // The reference's order: bottom left, top left, top right, bottom right.
                const usize order[4] = {3, 0, 1, 2};
                const f32 us[4] = {uv.x, uv.x, uv.x + uv.width, uv.x + uv.width};
                const f32 vs[4] = {uv.y + uv.height, uv.y, uv.y, uv.y + uv.height};
                const Vector2 faceUv[4] = {Vector2(0, 0), Vector2(0, 1), Vector2(1, 1), Vector2(1, 0)};
                VertexGradient gradientCorners = colorGradientPreset() ? colorGradientPreset()->Corners() : colorGradient_;
        const Color gradient[4] = {gradientCorners.bottomLeft, gradientCorners.topLeft, gradientCorners.topRight, gradientCorners.bottomRight};
                usize base = k * 4;
                for (usize v = 0; v < 4; v++)
                {
                    Vector2 p = corners[order[v]];
                    Vector3 local(left + p.x, top - p.y, 0);
                    m.vertices[base + v] = local;
                    m.uvs0[base + v] = Vector4(us[v], vs[v], kind, w);
                    Vector2 face = faceUv[v];
                    if (horizontalMapping_ == TextureMappingOptions::Paragraph || horizontalMapping_ == TextureMappingOptions::Line)
                        face.x = area.width > 0 ? (local.x - left) / area.width + mappingUvLineOffset_ : face.x;
                    if (verticalMapping_ == TextureMappingOptions::Paragraph || verticalMapping_ == TextureMappingOptions::Line)
                        face.y = area.height > 0 ? (local.y - area.y) / area.height : face.y;
                    m.uvs2[base + v] = face;
                    Color32 c = ToColor32(tint);
                    if (enableVertexGradient_ && !decor)
                        c = Multiply(c, gradient[v]);
                    m.colors32[base + v] = c;
                    m.normals[base + v] = Vector3(0, 0, -1);
                    m.tangents[base + v] = Vector4(-1, 0, 0, 1);
                    grow(local);
                }
                if (character != ~0u && character < info.characterInfo.Size())
                {
                    CharacterInfo& ci = info.characterInfo[character];
                    if (ci.vertexIndex < 0)
                    {
                        ci.vertexIndex = i32(base);
                        ci.materialReferenceIndex = i32(mi);
                        ci.material = m.material;
                        auto vertex = [&](usize v) {
                            Vertex out;
                            out.position = m.vertices[base + v];
                            out.uv = m.uvs0[base + v];
                            out.uv2 = m.uvs2[base + v];
                            out.color = m.colors32[base + v];
                            return out;
                        };
                        ci.vertex_BL = vertex(0), ci.vertex_TL = vertex(1), ci.vertex_TR = vertex(2), ci.vertex_BR = vertex(3);
                    }
                }
            }
            if (decor)
                m.material = engine::Canvas::GetDefaultCanvasMaterial();
            if (!m.material)
                m.material = fontSharedMaterial();
            m.SortGeometry(sortingOrder_);
        }
        info.materialCount = i32(info.meshInfo.Size());
        any = false;
        bmin = Vector3(std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(), 0);
        bmax = Vector3(-std::numeric_limits<f32>::max(), -std::numeric_limits<f32>::max(), 0);
        for (const CharacterInfo& c : info.characterInfo)
            if (c.isVisible && !IsSpace(c.character))
                grow(c.topLeft), grow(c.bottomRight), grow(c.topRight), grow(c.bottomLeft);
        if (any)
            bounds_ = Bounds(Vector3((bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f, 0), Vector3(bmax.x - bmin.x, bmax.y - bmin.y, 0));
        else
            bounds_ = Bounds();
        // The text's bounds: its lines' extents.
        bool lines = false;
        Vector2 tmin(0, 0), tmax(0, 0);
        for (const LineInfo& l : info.lineInfo)
        {
            if (!lines)
                tmin = l.lineExtents.min, tmax = l.lineExtents.max, lines = true;
            else
            {
                tmin = Vector2(l.lineExtents.min.x < tmin.x ? l.lineExtents.min.x : tmin.x, l.lineExtents.min.y < tmin.y ? l.lineExtents.min.y : tmin.y);
                tmax = Vector2(l.lineExtents.max.x > tmax.x ? l.lineExtents.max.x : tmax.x, l.lineExtents.max.y > tmax.y ? l.lineExtents.max.y : tmax.y);
            }
        }
        textBounds_ = lines ? Bounds(Vector3((tmin.x + tmax.x) * 0.5f, (tmin.y + tmax.y) * 0.5f, 0), Vector3(tmax.x - tmin.x, tmax.y - tmin.y, 0)) : Bounds();
        // The meshes from the arrays.
        for (MeshInfo& m : info.meshInfo)
            UpdateMeshGeometry(m.mesh, i32(&m - info.meshInfo.Data()));
    }

    void TextBase::UpdateMeshGeometry(engine::Mesh* mesh, i32 index)
    {
        if (!mesh || index < 0 || usize(index) >= info_->meshInfo.Size())
            return;
        const MeshInfo& m = info_->meshInfo[usize(index)];
        mesh->Clear();
        mesh->SetVertices(Span<const Vector3>(m.vertices.Data(), m.vertices.Size()));
        mesh->SetUVs(0, Span<const Vector4>(m.uvs0.Data(), m.uvs0.Size()));
        mesh->SetUVs(1, Span<const Vector2>(m.uvs2.Data(), m.uvs2.Size()));
        mesh->SetColors(Span<const Color32>(m.colors32.Data(), m.colors32.Size()));
        mesh->SetNormals(Span<const Vector3>(m.normals.Data(), m.normals.Size()));
        mesh->SetTangents(Span<const Vector4>(m.tangents.Data(), m.tangents.Size()));
        mesh->SetTriangles(Span<const i32>(m.triangles.Data(), m.triangles.Size()));
        mesh->RecalculateBounds();
    }

    void TextBase::UpdateVertexData(VertexDataUpdateFlags flags)
    {
        (void)flags;
        for (usize i = 0; i < info_->meshInfo.Size(); i++)
            UpdateMeshGeometry(info_->meshInfo[i].mesh, i32(i));
        UploadMeshes();
    }

    void TextBase::SetVertices(Span<const Vector3> vertices)
    {
        if (info_->meshInfo.Empty())
            return;
        MeshInfo& m = info_->meshInfo[0];
        for (usize i = 0; i < vertices.Size() && i < m.vertices.Size(); i++)
            m.vertices[i] = vertices[i];
        UpdateMeshGeometry(m.mesh, 0);
        UploadMeshes();
    }

    void TextBase::ClearMesh(bool uploadGeometry)
    {
        for (MeshInfo& m : info_->meshInfo)
            m.Clear(false);
        if (uploadGeometry)
        {
            mesh_->Clear();
            for (engine::Mesh* s : subMeshes_)
                s->Clear();
            UploadMeshes();
        }
    }

    void TextBase::ComputeMarginSize() { changed_ = true; }

    // ------------------------------------------------------------------------------------------------ queries

    TextInfo* TextBase::textInfo()
    {
        if (changed_ && IsActive())
            GenerateText();
        return info_;
    }

    Bounds TextBase::bounds()
    {
        textInfo();
        return bounds_;
    }

    Bounds TextBase::textBounds()
    {
        textInfo();
        return textBounds_;
    }

    String TextBase::GetParsedText()
    {
        textInfo();
        String out;
        for (const CharacterInfo& c : info_->characterInfo)
            AppendUtf8(out, u32(c.character));
        return out;
    }

    Vector2 TextBase::GetPreferredValues()
    {
        return GetPreferredValues(std::numeric_limits<f32>::infinity(), std::numeric_limits<f32>::infinity());
    }

    Vector2 TextBase::GetPreferredValues(f32 width, f32 height) { return GetPreferredValues(StringView(text_), width, height); }

    Vector2 TextBase::GetPreferredValues(StringView text)
    {
        return GetPreferredValues(text, std::numeric_limits<f32>::infinity(), std::numeric_limits<f32>::infinity());
    }

    Vector2 TextBase::GetPreferredValues(StringView text, f32 width, f32 height)
    {
        (void)height;
        ui::TextGenerationSettings s = Settings(-1, -1);
        if (!s.font)
            return Vector2(margin_.x + margin_.z, margin_.y + margin_.w);
        String source = parseCtrlCharacters_ ? ParseControlCharacters(text) : String(text);
        f32 inner = std::isfinite(width) ? width - margin_.x - margin_.z : -1.0f;
        if (wrapping_ == TextWrappingModes::NoWrap || wrapping_ == TextWrappingModes::PreserveWhitespaceNoWrap)
            inner = -1;
        Vector2 size = generator_.PreferredSize(source, s, inner);
        return Vector2(size.x + margin_.x + margin_.z, size.y + margin_.y + margin_.w);
    }

    f32 TextBase::preferredWidth() const { return const_cast<TextBase*>(this)->GetPreferredValues().x; }

    f32 TextBase::preferredHeight() const
    {
        auto* self = const_cast<TextBase*>(this);
        engine::RectTransform* r = rectTransform();
        return self->GetPreferredValues(r ? r->rect().width : std::numeric_limits<f32>::infinity(), std::numeric_limits<f32>::infinity()).y;
    }

    Vector2 TextBase::GetRenderedValues(bool onlyVisibleCharacters)
    {
        textInfo();
        if (!onlyVisibleCharacters)
            return Vector2(textBounds_.size().x, textBounds_.size().y);
        return Vector2(bounds_.size().x, bounds_.size().y);
    }

    TextInfo* TextBase::GetTextInfo(StringView text)
    {
        SetText(text);
        ForceMeshUpdate(true);
        return info_;
    }

    void TextBase::OnAfterCloneNative()
    {
        MaskableGraphic::OnAfterCloneNative();
        // Its own text info and meshes (the copied ones are the original's).
        sheets_.text = this;
        info_ = new TextInfo(this);
        mesh_ = engine::Mesh::Create();
        mesh_->SetName("Text Mesh");
        subMeshes_.Clear();
        meshTextures_.Clear();
        changed_ = true;
    }
}
