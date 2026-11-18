#include <Support/TextKit.h>

#include <Helix/CanvasUI/Text/CanvasText.h>
#include <Helix/CanvasUI/Text/TextMaterials.h>
#include <Helix/CanvasUI/Text/TextUtilities.h>
#include <Helix/CanvasUI/Text/WorldText.h>
#include <Helix/Testing/Test.h>

#include <cstdio>

using namespace helix;
using namespace helix::canvasui;
using namespace helix::canvasui::testing;
using engine::GameObject;

namespace
{
    CanvasText* MakeText(engine::Transform* parent, Vector2 size, ui::FontAsset* font, StringView text, f32 fontSize = 20)
    {
        GameObject* g = Element("Text", parent, Vector2(0, 0), size);
        CanvasText* t = Add<CanvasText>(g);
        t->SetFont(font);
        t->SetFontSize(fontSize);
        t->SetText(text);
        return t;
    }

    i32 QuadsOf(TextBase* t, usize mesh = 0)
    {
        TextInfo* info = t->textInfo();
        return mesh < info->meshInfo.Size() ? info->meshInfo[mesh].vertexCount / 4 : 0;
    }
}

HX_TEST(Text, CanvasTextLaysOutCharactersWordsLinesAndMeshes)
{
    HX_REQUIRE(RegisterModules());
    FontFolder folder;
    HX_REQUIRE(folder.font);
    ScreenSize(200, 100);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    // 160 x 40 around the center: x -80 to 80, y -20 to 20. Size 20: a glyph advances 9.6, a space 5.
    CanvasText* t = MakeText(canvas->transform(), Vector2(160, 40), folder.font, "Hello world");
    Frame();
    TextInfo* info = t->textInfo();
    HX_CHECK(info->characterCount == 11 && info->lineCount == 1 && info->wordCount == 2 && info->spaceCount == 1);
    const CharacterInfo& h = info->characterInfo[0];
    HX_CHECK(h.character == U'H' && h.isVisible && Near(h.origin, -80) && Near(info->characterInfo[1].origin, -80 + 9.6f) && !info->characterInfo[5].isVisible);
    HX_CHECK(Near(info->characterInfo[6].origin, -80 + 5 * 9.6f + 5) && info->characterInfo[6].lineNumber == 0);
    // The top line's baseline an ascender (16) below the top.
    HX_CHECK(Near(h.baseLine, 20 - 16, 0.05f) && Near(info->lineInfo[0].ascender, 20, 0.05f));
    // Ten glyph quads in the main mesh, the characters' vertices pointing into it.
    HX_CHECK(QuadsOf(t) == 10 && info->meshInfo.Size() == 1 && h.vertexIndex == 0 && info->characterInfo[6].vertexIndex == 20);
    HX_CHECK(t->mesh()->vertexCount() == 40 && t->canvasRenderer()->vertices().Size() > 0);
    HX_CHECK(t->GetParsedText() == StringView("Hello world") && info->wordInfo[1].GetWord() == StringView("world"));
    // Preferred: unwrapped 10 glyphs and a space; one line (ascender 16 + descender 4 + gap 1.8).
    Vector2 preferred = t->GetPreferredValues();
    HX_CHECK(Near(preferred.x, 10 * 9.6f + 5, 0.05f) && Near(t->preferredWidth(), preferred.x));
    // Wrapped in 60: two lines.
    Rt(t->gameObject())->SetSizeDelta(Vector2(60, 40));
    Frame();
    HX_CHECK(t->textInfo()->lineCount == 2 && t->textInfo()->characterInfo[6].lineNumber == 1 && Near(t->textInfo()->characterInfo[6].origin, -30));
    t->SetTextWrappingMode(TextWrappingModes::NoWrap);
    Frame();
    HX_CHECK(t->textInfo()->lineCount == 1 && t->isTextOverflowing());
    // Centered: the line in the middle.
    t->SetTextWrappingMode(TextWrappingModes::Normal);
    Rt(t->gameObject())->SetSizeDelta(Vector2(160, 40));
    t->SetAlignment(TextAlignmentOptions::Center);
    Frame();
    f32 width = 10 * 9.6f + 5;
    HX_CHECK(Near(t->textInfo()->characterInfo[0].origin, -width * 0.5f, 0.05f) && t->horizontalAlignment() == HorizontalAlignmentOptions::Center &&
             t->verticalAlignment() == VerticalAlignmentOptions::Middle);
    // Visible counts: the characters past them are not drawn (the layout stays).
    t->SetMaxVisibleCharacters(3);
    Frame();
    HX_CHECK(QuadsOf(t) == 3 && !t->textInfo()->characterInfo[3].isVisible && t->textInfo()->lineCount == 1);
    t->SetMaxVisibleCharacters(99999);
    t->SetMaxVisibleWords(1);
    Frame();
    HX_CHECK(QuadsOf(t) == 5);
    t->SetMaxVisibleWords(99999);
    // Rich text: bold and colored characters, the tags gone.
    t->SetText("<b>A</b><color=#FF0000>B</color>C");
    Frame();
    info = t->textInfo();
    HX_CHECK(info->characterCount == 3 && (i32(info->characterInfo[0].style) & i32(FontStyles::Bold)) != 0 && info->characterInfo[1].color == Color32(255, 0, 0, 255));
    HX_CHECK(t->isUsingBold() && t->GetParsedText() == StringView("ABC"));
    // Bold quads carry a negative distance field scale.
    HX_CHECK(info->meshInfo[0].uvs0[0].w < 0 && info->meshInfo[0].uvs0[4].w > 0);
    // Without rich text the tags stay.
    t->SetRichText(false);
    Frame();
    HX_CHECK(t->textInfo()->characterCount == 33);
    t->SetRichText(true);
    // Override the color tags: every character the text's color.
    t->SetOverrideColorTags(true);
    t->SetColor(Color(0, 0, 1, 1));
    Frame();
    HX_CHECK(t->textInfo()->characterInfo[1].color == Color32(0, 0, 255, 255));
    // The numbers of a format.
    t->SetTextFormat("Pi {0:2} n {1}", 3.14159f, 41.6f);
    HX_CHECK(t->text() == StringView("Pi 3.14 n 42"));
    // Escapes typed as text.
    t->SetText("a\\nb");
    Frame();
    HX_CHECK(t->textInfo()->lineCount == 2);
    DestroyImmediate(canvas->gameObject());
}

HX_TEST(Text, OverflowModesAutoSizeAndLinkedText)
{
    HX_REQUIRE(RegisterModules());
    FontFolder folder;
    HX_REQUIRE(folder.font);
    ScreenSize(200, 100);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    // Three lines of text in a box of one line.
    CanvasText* t = MakeText(canvas->transform(), Vector2(60, 22), folder.font, "aaaa bbbb cccc");
    Frame();
    HX_CHECK(t->textInfo()->lineCount == 3 && t->isTextOverflowing() && !t->isTextTruncated() && QuadsOf(t) == 12);
    // Truncate: only what fits is drawn; the first character that went is known.
    t->SetOverflowMode(TextOverflowModes::Truncate);
    Frame();
    HX_CHECK(t->isTextTruncated() && QuadsOf(t) == 4 && t->firstOverflowCharacterIndex() == 5);
    // Ellipsis: the last that fits replaced (one more quad: the ellipsis' dots).
    t->SetOverflowMode(TextOverflowModes::Ellipsis);
    Frame();
    HX_CHECK(t->isTextTruncated() && t->firstOverflowCharacterIndex() >= 0);
    // Page: one line a page, the second shown.
    t->SetOverflowMode(TextOverflowModes::Page);
    t->SetPageToDisplay(2);
    Frame();
    HX_CHECK(t->textInfo()->pageCount == 3 && QuadsOf(t) == 4 && t->textInfo()->characterInfo[5].isVisible && !t->textInfo()->characterInfo[0].isVisible);
    // Linked: the rest continues in the next text.
    CanvasText* next = MakeText(canvas->transform(), Vector2(60, 60), folder.font, "");
    t->SetOverflowMode(TextOverflowModes::Linked);
    t->SetLinkedTextComponent(next);
    t->SetPageToDisplay(1);
    t->ForceMeshUpdate();
    HX_CHECK(next->text() == StringView("bbbb cccc") && next->textInfo()->lineCount == 2);
    // Auto size: the largest size between min and max that fits.
    GameObject* g = Element("Auto", canvas->transform(), Vector2(0, 0), Vector2(100, 30));
    CanvasText* a = Add<CanvasText>(g);
    a->SetFont(folder.font);
    a->SetText("WWWWWWWWWW");
    a->SetEnableAutoSizing(true);
    a->SetFontSizeMin(4);
    a->SetFontSizeMax(40);
    a->ForceMeshUpdate();
    // Ten glyphs of 0.48 em in 100: 20.8.
    HX_CHECK(a->fontSize() > 19.5f && a->fontSize() < 21 && a->textInfo()->lineCount == 1);
    // The rendered size: the visible glyphs' bounds.
    Vector2 rendered = a->GetRenderedValues();
    HX_CHECK(rendered.x > 0 && rendered.x <= 100.5f && a->bounds().size().x > 0);
    DestroyImmediate(canvas->gameObject());
}

HX_TEST(Text, WorldTextDrawsItsMeshThroughItsRenderer)
{
    HX_REQUIRE(RegisterModules());
    FontFolder folder;
    HX_REQUIRE(folder.font);
    GameObject* g = engine::GameObject::Create("World Text");
    Rt(g) ? (void)0 : (void)g->AddComponent(engine::ClassOf<engine::RectTransform>());
    WorldText* t = Add<WorldText>(g);
    HX_REQUIRE(t && t->renderer() && t->meshFilter());
    Rt(g)->SetSizeDelta(Vector2(20, 5));
    t->SetFont(folder.font);
    // A point is a tenth of a unit in the world: 20 points, 2 units.
    t->SetFontSize(20);
    t->SetText("Sign");
    i32 events = 0;
    t->OnPreRenderText().Add([](void* u, TextInfo*) { ++*static_cast<i32*>(u); }, &events);
    Frame();
    HX_CHECK(events == 1 && t->textInfo()->characterCount == 4 && t->meshFilter()->sharedMesh() == t->mesh() && t->mesh()->vertexCount() == 16);
    HX_CHECK(t->renderer()->sharedMaterial() == TextMaterials::FontMaterial(folder.font));
    Vector2 size = t->GetRenderedValues();
    HX_CHECK(size.y > 1.0f && size.y < 2.5f);
    t->SetIsOrthographic(true);
    Frame();
    HX_CHECK(t->GetRenderedValues().y > 10.0f * 1.0f);
    t->SetIsOrthographic(false);
    // The font's material is the distance field shader's.
    HX_CHECK(t->fontSharedMaterial()->shader() && t->fontSharedMaterial()->shader()->Name() == StringView("Text/Distance Field"));
    t->SetSortingOrder(3);
    HX_CHECK(t->sortingOrder() == 3);
    DestroyImmediate(g);
}

HX_TEST(Text, SpritesStylesGradientsAndTheSettings)
{
    HX_REQUIRE(RegisterModules());
    FontFolder folder;
    HX_REQUIRE(folder.font);
    ScreenSize(200, 100);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    // A sprite asset of one sprite: a 32 pixel square of the sheet, as tall as the text.
    auto* sprites = static_cast<SpriteAsset*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<SpriteAsset>()));
    HX_REQUIRE(sprites);
    sprites->SetName("Icons");
    engine::Texture2D* sheet = FramedTexture(64, 4, Color32(255, 0, 0, 255), Color32(0, 0, 255, 255));
    sprites->SetSpriteSheet(sheet);
    HX_CHECK(sprites->AddSpriteInternal("smile", 0xE000, Rect(0, 0, 32, 32), 0, 32, 32, 1) == 0);
    HX_CHECK(sprites->GetSpriteIndexFromName("smile") == 0 && sprites->GetSpriteIndexFromUnicode(0xE000) == 0 && sprites->GetSpriteIndexFromName("frown") == -1);
    CanvasText* t = MakeText(canvas->transform(), Vector2(160, 40), folder.font, "a<sprite=0>b");
    t->SetSpriteAsset(sprites);
    Frame();
    TextInfo* info = t->textInfo();
    HX_REQUIRE(info->characterCount == 3);
    HX_CHECK(info->characterInfo[1].elementType == TextElementType::Sprite && info->spriteCount == 1 && info->meshInfo.Size() == 2 && QuadsOf(t, 1) == 1);
    // The sprite: 20 wide (as tall as the size), after the a.
    HX_CHECK(Near(info->characterInfo[2].origin - info->characterInfo[1].origin, 20, 0.05f));
    // By name, and from an asset the registry knows by its name.
    t->SetText("<sprite name=\"smile\">");
    Frame();
    HX_CHECK(t->textInfo()->spriteCount == 1);
    MaterialReferenceManager::AddSpriteAsset(sprites);
    t->SetSpriteAsset(nullptr);
    t->SetText("<sprite=\"Icons\" index=0>");
    Frame();
    HX_CHECK(t->textInfo()->spriteCount == 1 && QuadsOf(t, 1) == 1);
    // A style sheet: <style> stands for its tags; the text's own sheet, then the settings'.
    auto* styles = static_cast<TextStyleSheet*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<TextStyleSheet>()));
    styles->SetStyleInternal("Title", "<b><color=#00FF00>", "</color></b>");
    t->SetStyleSheet(styles);
    t->SetText("<style=Title>X</style>Y");
    Frame();
    info = t->textInfo();
    HX_CHECK(info->characterCount == 2 && (i32(info->characterInfo[0].style) & i32(FontStyles::Bold)) && info->characterInfo[0].color == Color32(0, 255, 0, 255));
    HX_CHECK(!(i32(info->characterInfo[1].style) & i32(FontStyles::Bold)));
    t->SetStyleSheet(nullptr);
    TextSettings::SetDefaultStyleSheet(styles);
    // The style of the whole text.
    t->SetText("Z");
    t->SetTextStyleHash(MaterialReferenceManager::HashOf("Title"));
    Frame();
    HX_CHECK(t->textInfo()->characterInfo[0].color == Color32(0, 255, 0, 255));
    t->SetTextStyleHash(-1183493901);
    // A gradient preset's corners on the vertices.
    auto* gradient = static_cast<TextColorGradient*>(engine::ScriptableObject::CreateInstance(scripting::TypeOf<TextColorGradient>()));
    gradient->SetColorMode(ColorMode::VerticalGradient);
    gradient->topLeft = Color(1, 0, 0, 1);
    gradient->bottomLeft = Color(0, 0, 1, 1);
    t->SetColorGradientPreset(gradient);
    t->SetEnableVertexGradient(true);
    t->SetText("H");
    Frame();
    const MeshInfo& m = t->textInfo()->meshInfo[0];
    HX_CHECK(m.colors32[0] == Color32(0, 0, 255, 255) && m.colors32[1] == Color32(255, 0, 0, 255));
    // Without a font: the settings' default; a <font> tag by the asset's name.
    GameObject* g = Element("Default", canvas->transform(), Vector2(0, 0), Vector2(100, 30));
    CanvasText* d = Add<CanvasText>(g);
    d->SetText("Hi");
    TextSettings::SetDefaultFontAsset(folder.font);
    d->ForceMeshUpdate();
    HX_CHECK(d->textInfo()->characterCount == 2 && d->font() == folder.font);
    folder.font->SetName("Page");
    MaterialReferenceManager::AddFontAsset(folder.font);
    d->SetText(Format("<font=\"{}\">A</font>", folder.font->Name()));
    d->ForceMeshUpdate();
    HX_CHECK(d->textInfo()->characterCount == 1 && d->textInfo()->characterInfo[0].fontAsset == folder.font);
    DestroyImmediate(canvas->gameObject());
    DestroyImmediate(sprites);
    DestroyImmediate(styles);
    DestroyImmediate(gradient);
    DestroyImmediate(sheet);
}

HX_TEST(Text, UtilitiesFindCharactersWordsLinesAndTheCaret)
{
    HX_REQUIRE(RegisterModules());
    FontFolder folder;
    HX_REQUIRE(folder.font);
    ScreenSize(200, 100);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    // "Hello world" at the top left of 160 x 40 around the screen's center: x from 20, the baseline at y 54.
    CanvasText* t = MakeText(canvas->transform(), Vector2(160, 40), folder.font, "Hello world");
    Frame();
    // In the H's glyph (x 21.2 to 28.1, y 54 to 59.2), between the words, past the end.
    HX_CHECK(TextUtilities::FindIntersectingCharacter(t, Vector3(24, 56, 0), nullptr, true) == 0);
    HX_CHECK(TextUtilities::FindIntersectingCharacter(t, Vector3(24, 30, 0), nullptr, true) == -1);
    HX_CHECK(TextUtilities::FindNearestCharacter(t, Vector3(24, 30, 0), nullptr, true) == 0);
    f32 worldX = 20 + 6 * 9.6f + 5 + 3;
    HX_CHECK(TextUtilities::FindIntersectingWord(t, Vector3(worldX, 56, 0), nullptr) == 1 && TextUtilities::FindIntersectingWord(t, Vector3(150, 56, 0), nullptr) == -1);
    HX_CHECK(TextUtilities::FindNearestWord(t, Vector3(150, 56, 0), nullptr) == 1);
    HX_CHECK(TextUtilities::FindIntersectingLine(t, Vector3(60, 56, 0), nullptr) == 0 && TextUtilities::FindIntersectingLine(t, Vector3(60, 10, 0), nullptr) == -1);
    HX_CHECK(TextUtilities::FindNearestLine(t, Vector3(60, 10, 0), nullptr) == 0);
    // The caret: before the first character at its left, after the last one past the end.
    HX_CHECK(TextUtilities::GetCursorIndexFromPosition(t, Vector3(21, 56, 0), nullptr) == 0);
    HX_CHECK(TextUtilities::GetCursorIndexFromPosition(t, Vector3(190, 56, 0), nullptr) == 11);
    CaretPosition caret;
    HX_CHECK(TextUtilities::GetCursorIndexFromPosition(t, Vector3(28, 56, 0), nullptr, caret) == 0 && caret == CaretPosition::Right);
    // Links by their text.
    t->SetText("a <link=\"id\">bc</link> d");
    Frame();
    HX_REQUIRE(t->textInfo()->linkCount == 1);
    HX_CHECK(t->textInfo()->linkInfo[0].GetLinkID() == StringView("id") && t->textInfo()->linkInfo[0].GetLinkText() == StringView("bc"));
    HX_CHECK(TextUtilities::FindIntersectingLink(t, Vector3(20 + 9.6f + 5 + 4, 56, 0), nullptr) == 0 && TextUtilities::FindIntersectingLink(t, Vector3(24, 56, 0), nullptr) == -1);
    // The rectangle under a point.
    HX_CHECK(TextUtilities::IsIntersectingRectTransform(t->rectTransform(), Vector3(100, 50, 0), nullptr) &&
             !TextUtilities::IsIntersectingRectTransform(t->rectTransform(), Vector3(5, 5, 0), nullptr));
    HX_CHECK(Near(TextUtilities::DistanceToLine(Vector3(0, 0, 0), Vector3(10, 0, 0), Vector3(5, 3, 0)), 3));
    DestroyImmediate(canvas->gameObject());
}
