#include <Helix/CanvasUI/Text/TextAssets.h>

#include <Helix/CanvasUI/Masking/Clipping.h>
#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Engine/Asset/Resources.h>
#include <Helix/UI/Engine/UIAssets.h>

namespace helix::canvasui
{
    // ------------------------------------------------------------------------------------------------ SpriteAsset

    Array<SpriteAsset*> SpriteAsset::FallbacksInternal() const
    {
        Array<SpriteAsset*> out;
        for (const engine::PPtr<engine::Object>& f : fallbacks_)
            out.Push(static_cast<SpriteAsset*>(f.Get()));
        return out;
    }

    void SpriteAsset::SetFallbacksInternal(Span<SpriteAsset* const> assets)
    {
        fallbacks_.Clear();
        for (SpriteAsset* a : assets)
            fallbacks_.Push(engine::PPtr<engine::Object>(a));
    }

    SpriteCharacterData SpriteAsset::CharacterInternal(i32 index) const
    {
        return index >= 0 && usize(index) < characters_.Size() ? characters_[usize(index)].data : SpriteCharacterData();
    }

    String SpriteAsset::CharacterNameInternal(i32 index) const
    {
        return index >= 0 && usize(index) < characters_.Size() ? characters_[usize(index)].name : String();
    }

    Array<SpriteGlyphData> SpriteAsset::GlyphsInternal() const
    {
        Array<SpriteGlyphData> out;
        for (const GlyphEntry& g : glyphs_)
            out.Push(g.data);
        return out;
    }

    void SpriteAsset::SetTablesInternal(Span<const SpriteCharacterData> characters, Span<const SpriteGlyphData> glyphs)
    {
        Array<Character> next;
        for (usize i = 0; i < characters.Size(); i++)
        {
            Character c;
            c.data = characters[i];
            c.name = i < characters_.Size() ? characters_[i].name : String();
            next.Push(std::move(c));
        }
        characters_ = std::move(next);
        glyphs_.Clear();
        for (const SpriteGlyphData& g : glyphs)
            glyphs_.Push(GlyphEntry{g});
    }

    void SpriteAsset::SetCharacterNameInternal(i32 index, StringView spriteName)
    {
        if (index < 0 || usize(index) >= characters_.Size())
            return;
        characters_[usize(index)].name = String(spriteName);
        characters_[usize(index)].data.hashCode = MaterialReferenceManager::HashOf(spriteName);
    }

    i32 SpriteAsset::AddSpriteInternal(StringView spriteName, u32 unicode, Rect rect, f32 bearingX, f32 bearingY, f32 advance, f32 scale)
    {
        GlyphEntry g;
        g.data.index = u32(glyphs_.Size());
        g.data.glyphRect = ui::GlyphRect{i32(rect.x), i32(rect.y), i32(rect.width), i32(rect.height)};
        g.data.metrics = ui::GlyphMetrics{rect.width, rect.height, bearingX, bearingY, advance};
        g.data.scale = scale;
        glyphs_.Push(g);
        Character c;
        c.data.unicode = unicode;
        c.data.glyphIndex = g.data.index;
        c.data.scale = 1;
        c.data.hashCode = MaterialReferenceManager::HashOf(spriteName);
        c.name = String(spriteName);
        characters_.Push(std::move(c));
        return i32(characters_.Size() - 1);
    }

    i32 SpriteAsset::GetSpriteIndexFromHashcode(i32 hash) const
    {
        for (usize i = 0; i < characters_.Size(); i++)
            if (characters_[i].data.hashCode == hash)
                return i32(i);
        return -1;
    }

    i32 SpriteAsset::GetSpriteIndexFromUnicode(u32 unicode) const
    {
        for (usize i = 0; i < characters_.Size(); i++)
            if (characters_[i].data.unicode == unicode)
                return i32(i);
        return -1;
    }

    i32 SpriteAsset::GetSpriteIndexFromName(StringView spriteName) const { return GetSpriteIndexFromHashcode(MaterialReferenceManager::HashOf(spriteName)); }

    void SpriteAsset::SortGlyphTable()
    {
        for (usize i = 1; i < glyphs_.Size(); i++)
            for (usize j = i; j > 0 && glyphs_[j].data.index < glyphs_[j - 1].data.index; j--)
                std::swap(glyphs_[j], glyphs_[j - 1]);
    }

    namespace
    {
        template <class F>
        SpriteAsset* Search(SpriteAsset* asset, bool includeFallbacks, i32& index, F find, u32 depth = 0)
        {
            index = -1;
            if (!asset || depth > 8)
                return nullptr;
            index = find(asset);
            if (index >= 0)
                return asset;
            if (includeFallbacks)
                for (SpriteAsset* f : asset->FallbacksInternal())
                    if (SpriteAsset* found = Search(f, true, index, find, depth + 1))
                        return found;
            return nullptr;
        }
    }

    SpriteAsset* SpriteAsset::SearchForSpriteByUnicode(SpriteAsset* spriteAsset, u32 unicode, bool includeFallbacks, i32& spriteIndex)
    {
        return Search(spriteAsset, includeFallbacks, spriteIndex, [&](SpriteAsset* a) { return a->GetSpriteIndexFromUnicode(unicode); });
    }

    SpriteAsset* SpriteAsset::SearchForSpriteByHashCode(SpriteAsset* spriteAsset, i32 hash, bool includeFallbacks, i32& spriteIndex)
    {
        return Search(spriteAsset, includeFallbacks, spriteIndex, [&](SpriteAsset* a) { return a->GetSpriteIndexFromHashcode(hash); });
    }

    bool SpriteAsset::Resolve(i32 index, f32 size, ui::TextSprite& out) const
    {
        if (index < 0 || usize(index) >= characters_.Size())
            return false;
        const Character& c = characters_[usize(index)];
        const GlyphEntry* g = nullptr;
        for (const GlyphEntry& e : glyphs_)
            if (e.data.index == c.data.glyphIndex)
                g = &e;
        engine::Texture* sheet = sheet_.Get();
        if (!g || !sheet)
            return false;
        const ui::GlyphMetrics& m = g->data.metrics;
        // Sheet pixels to the text's: the asset's point size, else a sprite as tall as the text.
        f32 reference = faceInfo_.pointSize > 0 ? faceInfo_.pointSize : (m.height > 0 ? m.height : 1);
        f32 k = size / reference * g->data.scale * c.data.scale;
        f32 w = f32(sheet->width()), h = f32(sheet->height());
        const ui::GlyphRect& r = g->data.glyphRect;
        out.texture = sheet;
        // The sheet's rows bottom first; the text's uvs run down from the top.
        out.uv = Rect(f32(r.x) / w, 1.0f - f32(r.y + r.height) / h, f32(r.width) / w, f32(r.height) / h);
        out.width = m.width * k;
        out.height = m.height * k;
        out.bearingX = m.horizontalBearingX * k;
        out.bearingY = m.horizontalBearingY * k;
        out.advance = m.horizontalAdvance * k;
        out.tint = false;
        return true;
    }

    // ------------------------------------------------------------------------------------------------ TextStyleSheet

    String TextStyleSheet::StyleNameInternal(i32 index) const { return index >= 0 && usize(index) < styles_.Size() ? styles_[usize(index)].name : String(); }

    String TextStyleSheet::StyleOpeningInternal(i32 index) const
    {
        return index >= 0 && usize(index) < styles_.Size() ? styles_[usize(index)].opening : String();
    }

    String TextStyleSheet::StyleClosingInternal(i32 index) const
    {
        return index >= 0 && usize(index) < styles_.Size() ? styles_[usize(index)].closing : String();
    }

    void TextStyleSheet::SetStyleInternal(StringView styleName, StringView opening, StringView closing)
    {
        for (Style& s : styles_)
            if (MaterialReferenceManager::HashOf(s.name) == MaterialReferenceManager::HashOf(styleName))
            {
                s.opening = String(opening);
                s.closing = String(closing);
                return;
            }
        styles_.Push(Style{String(styleName), String(opening), String(closing)});
    }

    i32 TextStyleSheet::FindStyleInternal(i32 hashCode) const
    {
        for (usize i = 0; i < styles_.Size(); i++)
            if (MaterialReferenceManager::HashOf(styles_[i].name) == hashCode)
                return i32(i);
        return -1;
    }

    bool TextStyleSheet::Find(StringView name, String& opening, String& closing) const
    {
        i32 i = FindStyleInternal(MaterialReferenceManager::HashOf(name));
        if (i < 0)
            return false;
        opening = styles_[usize(i)].opening;
        closing = styles_[usize(i)].closing;
        return true;
    }

    // ------------------------------------------------------------------------------------------------ TextColorGradient

    VertexGradient TextColorGradient::Corners() const
    {
        switch (colorMode_)
        {
        case ColorMode::Single: return VertexGradient(topLeft, topLeft, topLeft, topLeft);
        case ColorMode::HorizontalGradient: return VertexGradient(topLeft, topRight, topLeft, topRight);
        case ColorMode::VerticalGradient: return VertexGradient(topLeft, topLeft, bottomLeft, bottomLeft);
        default: return VertexGradient(topLeft, topRight, bottomLeft, bottomRight);
        }
    }

    // ------------------------------------------------------------------------------------------------ TextSettings

    namespace
    {
        engine::PPtr<engine::Object>& Instance()
        {
            static engine::PPtr<engine::Object>* p = new engine::PPtr<engine::Object>();
            return *p;
        }
    }

    TextSettings* TextSettings::instance()
    {
        if (auto* s = static_cast<TextSettings*>(Instance().Get()))
            return s;
        // The project's settings at the resources path, else defaults of their own.
        auto* loaded = static_cast<TextSettings*>(engine::Resources::Load("TMP Settings"));
        if (!loaded || !loaded->Is<TextSettings>())
            loaded = static_cast<TextSettings*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<TextSettings>()));
        Instance() = engine::PPtr<engine::Object>(loaded);
        return loaded;
    }

    void TextSettings::SetInstanceInternal(TextSettings* settings) { Instance() = engine::PPtr<engine::Object>(settings); }

    ui::FontAsset* TextSettings::defaultFontAsset() { return ui::ScriptObjectAs<ui::FontAsset>(instance()->font_.Get()); }
    void TextSettings::SetDefaultFontAsset(ui::FontAsset* value) { instance()->font_ = engine::PPtr<engine::Object>(value); }
    SpriteAsset* TextSettings::defaultSpriteAsset() { return static_cast<SpriteAsset*>(instance()->sprite_.Get()); }
    void TextSettings::SetDefaultSpriteAsset(SpriteAsset* value) { instance()->sprite_ = engine::PPtr<engine::Object>(value); }
    TextStyleSheet* TextSettings::defaultStyleSheet() { return static_cast<TextStyleSheet*>(instance()->styleSheet_.Get()); }
    void TextSettings::SetDefaultStyleSheet(TextStyleSheet* value) { instance()->styleSheet_ = engine::PPtr<engine::Object>(value); }

    Array<ui::FontAsset*> TextSettings::FallbackFontAssetsInternal()
    {
        Array<ui::FontAsset*> out;
        for (const engine::PPtr<engine::Object>& f : instance()->fallbacks_)
            if (auto* a = ui::ScriptObjectAs<ui::FontAsset>(f.Get()))
                out.Push(a);
        return out;
    }

    void TextSettings::SetFallbackFontAssetsInternal(Span<ui::FontAsset* const> fonts)
    {
        instance()->fallbacks_.Clear();
        for (ui::FontAsset* f : fonts)
            instance()->fallbacks_.Push(engine::PPtr<engine::Object>(f));
    }

    Array<engine::Object*> TextSettings::EmojiFallbackTextAssetsInternal()
    {
        Array<engine::Object*> out;
        for (const engine::PPtr<engine::Object>& f : instance()->emojiFallbacks_)
            out.Push(f.Get());
        return out;
    }

    void TextSettings::SetEmojiFallbackTextAssetsInternal(Span<engine::Object* const> assets)
    {
        instance()->emojiFallbacks_.Clear();
        for (engine::Object* a : assets)
            instance()->emojiFallbacks_.Push(engine::PPtr<engine::Object>(a));
    }

    // ------------------------------------------------------------------------------------------------ MaterialReferenceManager

    namespace
    {
        struct References
        {
            HashMap<i32, engine::PPtr<engine::Object>> fonts, sprites, materials, gradients;
        };

        References& Refs()
        {
            static References* r = new References();
            return *r;
        }

        template <class T>
        bool Get(HashMap<i32, engine::PPtr<engine::Object>>& map, i32 hash, T*& out)
        {
            out = nullptr;
            if (const engine::PPtr<engine::Object>* p = map.Find(hash))
                out = static_cast<T*>(p->Get());
            return out != nullptr;
        }
    }

    i32 MaterialReferenceManager::HashOf(StringView name)
    {
        // The reference's case insensitive hash of names (TMP_TextUtilities.GetHashCode).
        i32 hash = 0;
        for (char c : name)
        {
            char u = c >= 'a' && c <= 'z' ? char(c - 32) : c;
            hash = ((hash << 5) + hash) ^ i32(u8(u));
        }
        return hash;
    }

    void MaterialReferenceManager::AddFontAsset(ui::FontAsset* fontAsset)
    {
        if (fontAsset)
            Refs().fonts.Set(HashOf(fontAsset->Name()), engine::PPtr<engine::Object>(fontAsset));
    }

    void MaterialReferenceManager::AddSpriteAsset(SpriteAsset* spriteAsset)
    {
        if (spriteAsset)
            AddSpriteAsset(HashOf(spriteAsset->Name()), spriteAsset);
    }

    void MaterialReferenceManager::AddSpriteAsset(i32 hashCode, SpriteAsset* spriteAsset)
    {
        Refs().sprites.Set(hashCode, engine::PPtr<engine::Object>(spriteAsset));
    }

    void MaterialReferenceManager::AddFontMaterial(i32 hashCode, engine::Material* material)
    {
        Refs().materials.Set(hashCode, engine::PPtr<engine::Object>(material));
    }

    void MaterialReferenceManager::AddColorGradientPreset(i32 hashCode, TextColorGradient* spriteAsset)
    {
        Refs().gradients.Set(hashCode, engine::PPtr<engine::Object>(spriteAsset));
    }

    bool MaterialReferenceManager::TryGetFontAsset(i32 hashCode, ui::FontAsset*& fontAsset)
    {
        engine::Object* o = nullptr;
        bool found = Get(Refs().fonts, hashCode, o);
        fontAsset = found ? ui::ScriptObjectAs<ui::FontAsset>(o) : nullptr;
        return fontAsset != nullptr;
    }

    bool MaterialReferenceManager::TryGetSpriteAsset(i32 hashCode, SpriteAsset*& spriteAsset) { return Get(Refs().sprites, hashCode, spriteAsset); }
    bool MaterialReferenceManager::TryGetMaterial(i32 hashCode, engine::Material*& material) { return Get(Refs().materials, hashCode, material); }

    bool MaterialReferenceManager::TryGetColorGradientPreset(i32 hashCode, TextColorGradient*& gradientPreset)
    {
        return Get(Refs().gradients, hashCode, gradientPreset);
    }

    void MaterialReferenceManager::Clear()
    {
        Refs().fonts.Clear();
        Refs().sprites.Clear();
        Refs().materials.Clear();
        Refs().gradients.Clear();
    }

    // ------------------------------------------------------------------------------------------------ MaterialManager

    namespace
    {
        struct Fallback
        {
            engine::PPtr<engine::Material> source, target, made;
            i32 count = 0;
        };

        Array<Fallback>& Fallbacks()
        {
            static Array<Fallback>* f = new Array<Fallback>();
            return *f;
        }

        // The source's values on the target's atlas: everything but the texture and the atlas's own values.
        void CopyValues(engine::Material* source, engine::Material* destination)
        {
            for (const char* name : {"_FaceColor", "_OutlineColor", "_UnderlayColor"})
                destination->SetColor(name, source->GetColor(name));
            for (const char* name : {"_FaceDilate", "_OutlineWidth", "_OutlineSoftness", "_UnderlayOffsetX", "_UnderlayOffsetY", "_UnderlayDilate",
                                     "_UnderlaySoftness", "_WeightNormal", "_WeightBold", "_ScaleRatioA"})
                destination->SetFloat(name, source->GetFloat(name));
            if (source->IsKeywordEnabled("UNDERLAY_ON"))
                destination->EnableKeyword("UNDERLAY_ON");
            else
                destination->DisableKeyword("UNDERLAY_ON");
        }
    }

    engine::Material* MaterialManager::GetStencilMaterial(engine::Material* baseMaterial, i32 stencilID)
    {
        return StencilMaterial::Add(baseMaterial, (1 << stencilID) - 1, engine::StencilOp::Keep, engine::CompareFunction::Equal, engine::ColorWriteMask::All,
                                    (1 << stencilID) - 1, 0);
    }

    void MaterialManager::ReleaseStencilMaterial(engine::Material* stencilMaterial) { StencilMaterial::Remove(stencilMaterial); }

    engine::Material* MaterialManager::GetBaseMaterial(engine::Material* stencilMaterial) { return stencilMaterial; }

    engine::Material* MaterialManager::SetStencil(engine::Material* material, i32 stencilID)
    {
        if (!material)
            return nullptr;
        material->SetFloat("_Stencil", f32(stencilID));
        material->SetFloat("_StencilComp", stencilID == 0 ? 8.0f : 4.0f);
        return material;
    }

    i32 MaterialManager::GetStencilID(engine::GameObject* obj)
    {
        if (!obj)
            return 0;
        engine::Transform* t = obj->transform();
        i32 depth = MaskUtilities::GetStencilDepth(t, MaskUtilities::FindRootSortOverrideCanvas(t));
        return depth > 0 ? 1 << (depth - 1) : 0;
    }

    engine::Material* MaterialManager::GetFallbackMaterial(engine::Material* sourceMaterial, engine::Material* targetMaterial)
    {
        if (!sourceMaterial || !targetMaterial)
            return targetMaterial;
        for (Fallback& f : Fallbacks())
            if (f.source.Get() == sourceMaterial && f.target.Get() == targetMaterial && f.made.Get())
            {
                f.count++;
                CopyValues(sourceMaterial, f.made.Get());
                return f.made.Get();
            }
        engine::Material* made = engine::Material::Create(*targetMaterial);
        made->SetName(Format("{} + {}", sourceMaterial->Name(), targetMaterial->Name()));
        CopyValues(sourceMaterial, made);
        Fallbacks().Push(Fallback{engine::PPtr<engine::Material>(sourceMaterial), engine::PPtr<engine::Material>(targetMaterial),
                                  engine::PPtr<engine::Material>(made), 1});
        return made;
    }

    void MaterialManager::ReleaseFallbackMaterial(engine::Material* fallbackMaterial)
    {
        for (Fallback& f : Fallbacks())
            if (f.made.Get() == fallbackMaterial)
                f.count--;
    }

    void MaterialManager::CleanupFallbackMaterials()
    {
        Array<Fallback>& list = Fallbacks();
        for (usize i = 0; i < list.Size();)
        {
            if (list[i].count > 0 && list[i].made.Get())
            {
                i++;
                continue;
            }
            if (engine::Material* m = list[i].made.Get())
                engine::Object::DestroyImmediate(m);
            list.RemoveAt(i);
        }
    }

    void MaterialManager::CopyMaterialPresetProperties(engine::Material* source, engine::Material* destination)
    {
        if (source && destination)
            CopyValues(source, destination);
    }

    void MaterialManager::ClearMaterials()
    {
        for (Fallback& f : Fallbacks())
            f.count = 0;
        CleanupFallbackMaterials();
    }

    engine::Material* MaterialManager::GetMaterialForRendering(MaskableGraphic* graphic, engine::Material* baseMaterial)
    {
        return graphic ? graphic->GetModifiedMaterial(baseMaterial) : baseMaterial;
    }
}
