#include <Support/TextKit.h>

#include <Helix/CanvasUI/Controls/DefaultControls.h>
#include <Helix/CanvasUI/Controls/Dropdown.h>
#include <Helix/CanvasUI/Controls/TextInput.h>
#include <Helix/CanvasUI/Events/EventSystem.h>
#include <Helix/CanvasUI/Text/CanvasText.h>
#include <Helix/Engine/Input/SystemClipboard.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::canvasui;
using namespace helix::canvasui::testing;
using engine::GameObject;
using K = platform::KeyCode;

namespace
{
    struct Seen
    {
        Array<String> texts;
        Array<i32> values;
    };
    void RememberText(void* user, String v) { static_cast<Seen*>(user)->texts.Push(std::move(v)); }
    void RememberValue(void* user, i32 v) { static_cast<Seen*>(user)->values.Push(v); }

    struct Scene
    {
        EventSystem* es = nullptr;
        engine::Canvas* canvas = nullptr;

        Scene()
        {
            ScreenSize(200, 100);
            GameObject* g = engine::GameObject::Create("EventSystem");
            Add<StandaloneInputModule>(g);
            es = ComponentOf<EventSystem>(g);
            canvas = MakeCanvas("Canvas");
            Add<GraphicRaycaster>(canvas->gameObject());
        }
        ~Scene()
        {
            DestroyImmediate(canvas->gameObject());
            DestroyImmediate(es->gameObject());
        }
    };

    // The default input field (160 x 30 around the screen's center) in the page font.
    TextInput* MakeInput(Scene& scene, ui::FontAsset* font)
    {
        GameObject* g = TextDefaultControls::CreateInputField({});
        g->transform()->SetParent(scene.canvas->transform(), false);
        auto* input = ComponentOf<TextInput>(g);
        input->SetGlobalFontAsset(font);
        input->SetCaretBlinkRate(0);
        return input;
    }

    String Displayed(TextInput* input) { return String(input->textComponent()->text()); }
}

HX_TEST(TextControls, TextInputTypesEditsSelectsAndSubmits)
{
    HX_REQUIRE(RegisterModules());
    FontFolder folder;
    HX_REQUIRE(folder.font);
    {
        Scene scene;
        TextInput* input = MakeInput(scene, folder.font);
        Seen changed, ended, submitted;
        input->onValueChanged().listeners().Add(&RememberText, &changed);
        input->onEndEdit().listeners().Add(&RememberText, &ended);
        input->onSubmit().listeners().Add(&RememberText, &submitted);
        InputFrames frames;
        frames.Move(Vector2(5, 5)).Run();
        HX_CHECK(!input->isFocused());
        // A click focuses it (at the next late update); the keys of the frame after go in.
        frames.Move(Vector2(100, 50)).Down(Vector2(100, 50)).Up(Vector2(100, 50)).Run();
        HX_CHECK(input->isFocused() && scene.es->currentSelectedGameObject() == input->gameObject());
        frames.Type("Hello").Run();
        HX_CHECK(input->text() == StringView("Hello") && input->caretPosition() == 5 && Displayed(input) == StringView("Hello"));
        // A change per character.
        HX_CHECK(changed.texts.Size() == 5 && changed.texts[0] == StringView("H") && changed.texts[4] == StringView("Hello"));
        // The caret moves; a character goes in at it.
        frames.Press(K::LeftArrow).Press(K::LeftArrow).Type("X").Run();
        HX_CHECK(input->text() == StringView("HelXlo") && input->caretPosition() == 4);
        // Backspace and delete around the caret.
        frames.Press(K::Backspace).Press(K::Delete).Run();
        HX_CHECK(input->text() == StringView("Helo") && input->caretPosition() == 3);
        // Shift extends the selection; typing replaces it.
        frames.Press(K::Home).Press(K::RightArrow, platform::ModShift).Press(K::RightArrow, platform::ModShift).Run();
        HX_CHECK(input->selectionAnchorPosition() == 0 && input->selectionFocusPosition() == 2);
        frames.Type("J").Run();
        HX_CHECK(input->text() == StringView("Jlo") && input->caretPosition() == 1 && input->selectionAnchorPosition() == 1);
        // Select all, copy, paste at the end: the clipboard's text.
        frames.Press(K::A, platform::ModControl).Press(K::C, platform::ModControl).Press(K::End).Press(K::V, platform::ModControl).Run();
        HX_CHECK(engine::SystemClipboard::Text() == StringView("Jlo") && input->text() == StringView("JloJlo"));
        // Cut the first word's worth.
        frames.Press(K::Home).Press(K::RightArrow, platform::ModShift).Press(K::X, platform::ModControl).Run();
        HX_CHECK(engine::SystemClipboard::Text() == StringView("J") && input->text() == StringView("loJlo"));
        // Return on a single line submits and ends the editing.
        frames.Press(K::Return).Run();
        HX_CHECK(!input->isFocused() && submitted.texts.Size() == 1 && submitted.texts[0] == StringView("loJlo"));
        HX_CHECK(ended.texts.Size() == 1 && ended.texts[0] == StringView("loJlo"));

        // Escape restores the text of the activation (which selects all: the caret to the end first).
        input->ActivateInputField();
        frames.Run();
        HX_CHECK(input->isFocused() && input->selectionAnchorPosition() == 0 && input->caretPosition() == 5);
        frames.Press(K::End).Type("zz").Run();
        HX_CHECK(input->text() == StringView("loJlozz"));
        frames.Press(K::Escape).Run();
        HX_CHECK(!input->isFocused() && input->wasCanceled() && input->text() == StringView("loJlo"));

        // Read only: selectable, not editable.
        input->SetReadOnly(true);
        input->ActivateInputField();
        frames.Run();
        frames.Type("q").Press(K::Backspace).Run();
        HX_CHECK(input->text() == StringView("loJlo"));
        input->SetReadOnly(false);
        input->DeactivateInputField();
        frames.Run();
        engine::SystemClipboard::SetText("");
    }
    engine::testing::Cleanup();
}

HX_TEST(TextControls, TextInputValidatesLimitsHidesAndComposes)
{
    HX_REQUIRE(RegisterModules());
    FontFolder folder;
    HX_REQUIRE(folder.font);
    {
        Scene scene;
        TextInput* input = MakeInput(scene, folder.font);
        InputFrames frames;
        frames.Run();
        input->SetOnFocusSelectAll(false);
        input->ActivateInputField();
        frames.Run();
        HX_REQUIRE(input->isFocused());

        // An integer: digits and a leading minus only.
        input->SetContentType(TextInput::ContentType::IntegerNumber);
        frames.Type("-1a2-3").Run();
        HX_CHECK(input->text() == StringView("-123"));
        // A decimal: one point.
        input->SetText("");
        input->SetContentType(TextInput::ContentType::DecimalNumber);
        frames.Type("1.5.2x").Run();
        HX_CHECK(input->text() == StringView("1.52"));
        // Alphanumeric.
        input->SetText("");
        input->SetContentType(TextInput::ContentType::Alphanumeric);
        frames.Type("a1 b-2").Run();
        HX_CHECK(input->text() == StringView("a1b2"));
        // A name: capitalized words.
        input->SetText("");
        input->SetContentType(TextInput::ContentType::Name);
        frames.Type("ada lovelace").Run();
        HX_CHECK(input->text() == StringView("Ada Lovelace"));
        // A pattern of one character class.
        input->SetText("");
        input->SetContentType(TextInput::ContentType::Standard);
        input->SetCharacterValidation(TextInput::CharacterValidation::Regex);
        input->SetRegexValue("[a-c]");
        frames.Type("abxcd").Run();
        HX_CHECK(input->text() == StringView("abc"));
        input->SetCharacterValidation(TextInput::CharacterValidation::None);

        // The character limit.
        input->SetText("");
        input->SetCharacterLimit(4);
        frames.Type("abcdef").Run();
        HX_CHECK(input->text() == StringView("abcd"));
        input->SetCharacterLimit(0);

        // A password shows asterisks; copy is refused.
        input->SetText("");
        input->SetContentType(TextInput::ContentType::Password);
        frames.Type("pw12").Run();
        HX_CHECK(input->text() == StringView("pw12") && Displayed(input) == StringView("****"));
        engine::SystemClipboard::SetText("kept");
        frames.Press(K::A, platform::ModControl).Press(K::C, platform::ModControl).Run();
        HX_CHECK(engine::SystemClipboard::Text() == StringView("kept"));
        input->SetContentType(TextInput::ContentType::Standard);

        // Multi line: Return is a new line; up and down keep the column.
        input->SetText("");
        input->SetLineType(TextInput::LineType::MultiLineNewline);
        frames.Type("abc").Press(K::Return).Type("de").Run();
        HX_CHECK(input->isFocused() && input->text() == StringView("abc\nde") && input->caretPosition() == 6);
        frames.Press(K::UpArrow).Run();
        HX_CHECK(input->caretPosition() == 2);
        frames.Press(K::DownArrow).Run();
        HX_CHECK(input->caretPosition() == 6);
        input->SetLineType(TextInput::LineType::SingleLine);

        // The input method's composition shows at the caret, not in the text; its result comes as text.
        input->SetText("ab");
        input->MoveTextEnd(false);
        frames.Compose("ka").Run();
        HX_CHECK(input->text() == StringView("ab") && Displayed(input) == StringView("abka"));
        frames.Compose("").Type("K").Run();
        HX_CHECK(input->text() == StringView("abK") && Displayed(input) == StringView("abK"));

        // The text set by a script raises the change; without notify it does not.
        Seen changed;
        input->onValueChanged().listeners().Add(&RememberText, &changed);
        input->SetText("set");
        input->SetTextWithoutNotify("quiet");
        HX_CHECK(changed.texts.Size() == 1 && changed.texts[0] == StringView("set") && input->text() == StringView("quiet"));
        input->onValueChanged().listeners().Remove(&RememberText, &changed);
        input->DeactivateInputField();
        frames.Run();
        engine::SystemClipboard::SetText("");
    }
    engine::testing::Cleanup();
}

HX_TEST(TextControls, DropdownShowsPicksAndHidesItsList)
{
    HX_REQUIRE(RegisterModules());
    FontFolder folder;
    HX_REQUIRE(folder.font);
    {
        Scene scene;
        GameObject* g = TextDefaultControls::CreateDropdown({});
        g->transform()->SetParent(scene.canvas->transform(), false);
        // Low on a 200 x 200 screen (25 to 55): the list (68 tall once fitted to its items) leaves the screen below and
        // flips over the dropdown.
        ScreenSize(200, 200);
        Rt(g)->SetAnchoredPosition(Vector2(0, -60));
        auto* dropdown = ComponentOf<Dropdown>(g);
        dropdown->SetAlphaFadeSpeed(0);
        for (CanvasText* t : {static_cast<CanvasText*>(dropdown->captionText()), static_cast<CanvasText*>(dropdown->itemText())})
            t->SetFont(folder.font);
        Seen seen;
        dropdown->onValueChanged().listeners().Add(&RememberValue, &seen);

        InputFrames frames;
        frames.Run();
        HX_CHECK(dropdown->value() == 0 && dropdown->captionText()->text() == StringView("Option A") && !dropdown->isExpanded());
        HX_CHECK(!dropdown->templateRect()->gameObject()->activeSelf());

        // A click shows the list: an item per option under its own sorting canvas, over a blocker.
        frames.Move(Vector2(100, 40)).Down(Vector2(100, 40)).Up(Vector2(100, 40)).Run();
        HX_REQUIRE(dropdown->isExpanded());
        engine::Transform* list = nullptr;
        for (u32 i = 0; i < g->transform()->childCount(); i++)
            if (g->transform()->GetChild(i)->gameObject()->Name() == StringView("Dropdown List"))
                list = g->transform()->GetChild(i);
        HX_REQUIRE(list);
        auto* listCanvas = list->gameObject()->GetComponent<engine::Canvas>();
        HX_CHECK(listCanvas && listCanvas->overrideSorting() && listCanvas->sortingOrder() == 30000);
        Array<engine::Component*> items = list->gameObject()->GetComponentsInChildren(scripting::TypeOf<DropdownItem>(), false);
        HX_CHECK(items.Size() == 3);
        auto* second = static_cast<DropdownItem*>(items[1]);
        HX_CHECK(second->text()->text() == StringView("Option B") && second->gameObject()->Name() == StringView("Item 1: Option B"));
        HX_CHECK(static_cast<DropdownItem*>(items[0])->toggle()->isOn() && !second->toggle()->isOn());
        engine::GameObject* blocker = nullptr;
        for (u32 i = 0; i < scene.canvas->transform()->childCount(); i++)
            if (scene.canvas->transform()->GetChild(i)->gameObject()->Name() == StringView("Blocker"))
                blocker = scene.canvas->transform()->GetChild(i)->gameObject();
        HX_REQUIRE(blocker);
        HX_CHECK(blocker->GetComponent<engine::Canvas>()->sortingOrder() == 29999);
        // The list leaves the canvas below: flipped above the dropdown (its pivot on the bottom edge).
        HX_CHECK(Near(static_cast<engine::RectTransform*>(list)->pivot().y, 0));

        // A click on the second item picks it and closes the list.
        Vector3 corners[4];
        second->rectTransform()->GetWorldCorners(corners);
        Vector2 at((corners[0].x + corners[2].x) * 0.5f, (corners[0].y + corners[2].y) * 0.5f);
        // The pointer over it first (its enter selects it), then the click.
        frames.Move(at).Run();
        frames.Down(at).Up(at).Run();
        frames.Run();
        HX_CHECK(dropdown->value() == 1 && dropdown->captionText()->text() == StringView("Option B") && !dropdown->isExpanded());
        HX_CHECK(seen.values.Size() == 1 && seen.values[0] == 1);
        HX_CHECK(scene.es->currentSelectedGameObject() == g);

        // A click on the blocker closes it with no change.
        frames.Move(Vector2(100, 40)).Down(Vector2(100, 40)).Up(Vector2(100, 40)).Run();
        HX_CHECK(dropdown->isExpanded());
        frames.Move(Vector2(3, 3)).Down(Vector2(3, 3)).Up(Vector2(3, 3)).Run();
        frames.Run();
        HX_CHECK(!dropdown->isExpanded() && dropdown->value() == 1);

        // Submit shows it, cancel hides it.
        frames.Press(K::Return).Run();
        HX_CHECK(dropdown->isExpanded());
        frames.Press(K::Escape).Run();
        frames.Run();
        HX_CHECK(!dropdown->isExpanded());

        // The value clamps to the options; without notify no event.
        dropdown->SetValue(7);
        HX_CHECK(dropdown->value() == 2 && dropdown->captionText()->text() == StringView("Option C") && seen.values.Size() == 2);
        dropdown->SetValueWithoutNotify(0);
        HX_CHECK(dropdown->value() == 0 && seen.values.Size() == 2);

        // A placeholder: shown at -1 and with no options.
        GameObject* ph = Element("Placeholder", g->transform(), Vector2(0, 0), Vector2(100, 20));
        CanvasText* placeholder = Add<CanvasText>(ph);
        dropdown->SetPlaceholder(placeholder);
        HX_CHECK(!placeholder->enabled());
        dropdown->SetValue(-1);
        HX_CHECK(dropdown->value() == -1 && placeholder->enabled() && dropdown->captionText()->text() == StringView(""));
        dropdown->ClearOptions();
        HX_CHECK(dropdown->value() == -1 && dropdown->OptionCountInternal() == 0 && placeholder->enabled());
        dropdown->onValueChanged().listeners().Remove(&RememberValue, &seen);
    }
    engine::testing::Cleanup();
}

HX_TEST(TextControls, DropdownMultiSelectFlipsBits)
{
    HX_REQUIRE(RegisterModules());
    FontFolder folder;
    HX_REQUIRE(folder.font);
    {
        Scene scene;
        GameObject* g = TextDefaultControls::CreateDropdown({});
        g->transform()->SetParent(scene.canvas->transform(), false);
        Rt(g)->SetAnchoredPosition(Vector2(0, 20));
        auto* dropdown = ComponentOf<Dropdown>(g);
        dropdown->SetAlphaFadeSpeed(0);
        dropdown->SetMultiSelect(true);
        dropdown->SetValueWithoutNotify(0);
        dropdown->RefreshShownValue();
        HX_CHECK(dropdown->captionText()->text() == StringView("Nothing"));
        dropdown->SetValue(2);
        HX_CHECK(dropdown->captionText()->text() == StringView("Option B"));
        dropdown->SetValue(3);
        HX_CHECK(dropdown->captionText()->text() == StringView("Mixed..."));
        dropdown->SetValue(7);
        HX_CHECK(dropdown->captionText()->text() == StringView("Everything"));

        // The list: Nothing, Everything, then the options; a pick flips its bit and the list stays.
        dropdown->SetValueWithoutNotify(1);
        InputFrames frames;
        frames.Run();
        dropdown->Show();
        HX_REQUIRE(dropdown->isExpanded());
        Array<engine::Component*> items;
        for (u32 i = 0; i < g->transform()->childCount(); i++)
            if (g->transform()->GetChild(i)->gameObject()->Name() == StringView("Dropdown List"))
                items = g->transform()->GetChild(i)->gameObject()->GetComponentsInChildren(scripting::TypeOf<DropdownItem>(), false);
        HX_REQUIRE(items.Size() == 5);
        auto item = [&](usize i) { return static_cast<DropdownItem*>(items[i]); };
        HX_CHECK(item(0)->text()->text() == StringView("Nothing") && item(1)->text()->text() == StringView("Everything"));
        HX_CHECK(!item(0)->toggle()->isOn() && !item(1)->toggle()->isOn() && item(2)->toggle()->isOn() && !item(3)->toggle()->isOn());
        item(3)->toggle()->SetIsOn(true);
        HX_CHECK(dropdown->value() == 3 && dropdown->isExpanded() && item(3)->toggle()->isOn());
        item(4)->toggle()->SetIsOn(true);
        HX_CHECK(dropdown->value() == 7 && item(1)->toggle()->isOn());
        item(2)->toggle()->SetIsOn(false);
        HX_CHECK(dropdown->value() == 6 && !item(1)->toggle()->isOn() && !item(2)->toggle()->isOn());
        // Nothing clears every bit; Everything sets them.
        item(0)->toggle()->SetIsOn(true);
        HX_CHECK(dropdown->value() == 0 && !item(3)->toggle()->isOn() && !item(4)->toggle()->isOn() && item(0)->toggle()->isOn());
        item(1)->toggle()->SetIsOn(true);
        HX_CHECK(dropdown->value() == 7 && item(2)->toggle()->isOn() && item(4)->toggle()->isOn() && !item(0)->toggle()->isOn());
        dropdown->Hide();
        frames.Run();
        HX_CHECK(!dropdown->isExpanded());
    }
    engine::testing::Cleanup();
}

HX_TEST(TextControls, DefaultControlsComeWithTheirLabels)
{
    HX_REQUIRE(RegisterModules());
    {
        DefaultControls::Resources resources;
        GameObject* button = DefaultControls::CreateButton(resources);
        HX_REQUIRE(button->transform()->childCount() == 1);
        auto* label = ComponentOf<CanvasText>(button->transform()->GetChild(0)->gameObject());
        HX_CHECK(label && label->text() == StringView("Button") && label->alignment() == TextAlignmentOptions::Center && Near(label->fontSize(), 14));
        GameObject* toggle = DefaultControls::CreateToggle(resources);
        HX_REQUIRE(toggle->transform()->childCount() == 2);
        auto* toggleLabel = ComponentOf<CanvasText>(toggle->transform()->GetChild(1)->gameObject());
        HX_CHECK(toggleLabel && toggleLabel->text() == StringView("Toggle") && Near(Rt(toggleLabel->gameObject())->offsetMin(), Vector2(23, 1)));

        GameObject* text = TextDefaultControls::CreateText(resources);
        HX_CHECK(text->Name() == StringView("Text (TMP)") && Near(Rt(text)->sizeDelta(), Vector2(200, 50)) && ComponentOf<CanvasText>(text)->text() == StringView("New Text"));
        GameObject* textButton = TextDefaultControls::CreateButton(resources);
        HX_CHECK(textButton->transform()->GetChild(0)->gameObject()->Name() == StringView("Text (TMP)"));
        GameObject* field = TextDefaultControls::CreateInputField(resources);
        auto* input = ComponentOf<TextInput>(field);
        HX_CHECK(field->Name() == StringView("InputField (TMP)") && input && input->textComponent() && input->placeholder() && input->textViewport());
        HX_CHECK(static_cast<TextBase*>(input->placeholder())->text() == StringView("Enter text..."));
        GameObject* dropdown = TextDefaultControls::CreateDropdown(resources);
        auto* d = ComponentOf<Dropdown>(dropdown);
        HX_CHECK(d && d->OptionCountInternal() == 3 && d->captionText() && d->itemText() && d->templateRect() && !d->templateRect()->gameObject()->activeSelf());
        GameObject* bar = TextDefaultControls::CreateScrollbar(resources);
        HX_CHECK(bar->Name() == StringView("Scrollbar"));
        for (GameObject* o : {button, toggle, text, textButton, field, dropdown, bar})
            DestroyImmediate(o);
    }
    engine::testing::Cleanup();
}

HX_TEST(TextControls, InstantiatedControlsOwnTheirState)
{
    HX_REQUIRE(RegisterModules());
    FontFolder folder;
    HX_REQUIRE(folder.font);
    {
        Scene scene;
        TextInput* input = MakeInput(scene, folder.font);
        input->SetText("copy me");
        GameObject* d = TextDefaultControls::CreateDropdown({});
        d->transform()->SetParent(scene.canvas->transform(), false);
        InputFrames frames;
        frames.Run();
        i32 selectables = Selectable::allSelectableCount();

        // The copies: their own text info, meshes and animation triggers; the serialized values copied.
        auto* inputCopy = static_cast<TextInput*>(engine::Object::Instantiate(input, scene.canvas->transform()));
        auto* dropdownCopy = static_cast<Dropdown*>(engine::Object::Instantiate(ComponentOf<Dropdown>(d), scene.canvas->transform()));
        HX_REQUIRE(inputCopy && dropdownCopy);
        frames.Run();
        HX_CHECK(Selectable::allSelectableCount() == selectables + 2);
        HX_CHECK(inputCopy->text() == StringView("copy me") && inputCopy->textComponent() != input->textComponent());
        TextBase* text = input->textComponent();
        TextBase* textCopy = inputCopy->textComponent();
        HX_CHECK(textCopy->textInfo() != text->textInfo() && textCopy->mesh() != text->mesh() && textCopy->textInfo()->textComponent == textCopy);
        HX_CHECK(inputCopy->animationTriggers() != input->animationTriggers());
        HX_CHECK(dropdownCopy->OptionCountInternal() == 3 && dropdownCopy->templateRect() != ComponentOf<Dropdown>(d)->templateRect());

        // The copy's list opens and closes on its own.
        dropdownCopy->SetAlphaFadeSpeed(0);
        dropdownCopy->Show();
        HX_CHECK(dropdownCopy->isExpanded() && !ComponentOf<Dropdown>(d)->isExpanded());
        dropdownCopy->Hide();
        frames.Run();
        frames.Run();
        HX_CHECK(!dropdownCopy->isExpanded());

        // Gone: the originals' state intact.
        DestroyImmediate(inputCopy->gameObject());
        DestroyImmediate(dropdownCopy->gameObject());
        HX_CHECK(Selectable::allSelectableCount() == selectables);
        input->SetText("still mine");
        frames.Run();
        HX_CHECK(text->textInfo()->characterCount == 10 && text->textInfo()->textComponent == text);
    }
    engine::testing::Cleanup();
}
