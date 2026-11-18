#include <Helix/CanvasUI/Text/TextMaterials.h>

#include <Helix/Core/Containers/HashMap.h>
#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/UI/Engine/UIAssets.h>

namespace helix::canvasui
{
    namespace
    {
        struct Entry
        {
            engine::PPtr<engine::Object> font;
            engine::PPtr<engine::Material> material;
        };

        Array<Entry>& Entries()
        {
            static Array<Entry>* e = new Array<Entry>();
            return *e;
        }
    }

    void TextMaterials::ApplyFont(engine::Material* material, ui::FontAsset* font)
    {
        if (!material || !font)
            return;
        material->SetFloat("_GradientScale", f32(font->atlasPadding() + 1));
        material->SetFloat("_TextureWidth", f32(font->atlasWidth()));
        material->SetFloat("_TextureHeight", f32(font->atlasHeight()));
    }

    engine::Material* TextMaterials::FontMaterial(ui::FontAsset* font)
    {
        if (!font)
            return nullptr;
        // The font asset's own material first.
        if (engine::Material* own = font->material())
            return own;
        Array<Entry>& entries = Entries();
        for (usize i = 0; i < entries.Size();)
        {
            if (!entries[i].font.Get() || !entries[i].material.Get())
            {
                entries.RemoveAt(i);
                continue;
            }
            if (entries[i].font.Get() == font)
                return entries[i].material.Get();
            i++;
        }
        engine::Shader* shader = engine::Shader::Find(kDistanceFieldShader);
        engine::Material* m = shader ? engine::Material::Create(shader) : engine::Material::Create(*engine::Canvas::GetDefaultCanvasMaterial());
        m->SetName(Format("{} Material", font->Name()));
        ApplyFont(m, font);
        entries.Push(Entry{engine::PPtr<engine::Object>(font), engine::PPtr<engine::Material>(m)});
        // The asset keeps it (its material from now on).
        font->SetMaterial(m);
        return m;
    }

    void TextMaterials::SetFontMaterial(ui::FontAsset* font, engine::Material* material)
    {
        if (font)
            font->SetMaterial(material);
        Array<Entry>& entries = Entries();
        for (usize i = 0; i < entries.Size(); i++)
            if (entries[i].font.Get() == font)
            {
                entries.RemoveAt(i);
                break;
            }
        if (font && material)
            entries.Push(Entry{engine::PPtr<engine::Object>(font), engine::PPtr<engine::Material>(material)});
    }

    void TextMaterials::Clear()
    {
        for (const Entry& e : Entries())
        {
            if (auto* f = ui::ScriptObjectAs<ui::FontAsset>(e.font.Get()); f && f->material() == e.material.Get())
                f->SetMaterial(nullptr);
            if (engine::Material* m = e.material.Get())
                engine::Object::DestroyImmediate(m);
        }
        Entries().Clear();
    }
}
