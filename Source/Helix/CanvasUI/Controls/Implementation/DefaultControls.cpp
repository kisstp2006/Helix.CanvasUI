#include <Helix/CanvasUI/Controls/DefaultControls.h>

#include <Helix/CanvasUI/Controls/Dropdown.h>
#include <Helix/CanvasUI/Controls/ScrollRect.h>
#include <Helix/CanvasUI/Controls/Sliders.h>
#include <Helix/CanvasUI/Controls/TextInput.h>
#include <Helix/CanvasUI/Layout/LayoutGroups.h>
#include <Helix/CanvasUI/Masking/Masks.h>
#include <Helix/CanvasUI/Text/CanvasText.h>
#include <Helix/CanvasUI/Rendering/Image.h>
#include <Helix/Engine/Transform/RectTransform.h>

#include <initializer_list>

namespace helix::canvasui
{
    namespace
    {
        constexpr f32 kWidth = 160, kThickHeight = 30, kThinHeight = 20;
        const Vector2 kThickElementSize(kWidth, kThickHeight);
        const Vector2 kThinElementSize(kWidth, kThinHeight);
        const Vector2 kImageElementSize(100, 100);
        const Color kDefaultSelectableColor(1, 1, 1, 1);
        const Color kPanelColor(1, 1, 1, 0.392f);

        // The default factory: a plain GameObject with the components (each adds the ones it requires first).
        class RuntimeFactory : public IFactoryControls
        {
        public:
            engine::GameObject* CreateGameObject(StringView name, Span<const scripting::TypeId> components) override
            {
                engine::GameObject* g = engine::GameObject::Create(name);
                for (scripting::TypeId t : components)
                    g->AddComponent(t);
                return g;
            }
        };

        // A factory of C#: the name and the types read back by the C# side, which answers the GameObject.
        class ManagedFactory : public IFactoryControls
        {
        public:
            engine::GameObject* CreateGameObject(StringView name, Span<const scripting::TypeId> components) override
            {
                String savedName = name_;
                Array<scripting::TypeId> savedTypes = types_;
                engine::PPtr<engine::GameObject> savedCreated = created_;
                name_ = String(name);
                types_.Clear();
                for (scripting::TypeId t : components)
                    types_.Push(t);
                created_ = engine::PPtr<engine::GameObject>();
                DefaultControls::managedCreate().Invoke();
                engine::GameObject* made = created_.Get();
                name_ = savedName, types_ = savedTypes, created_ = savedCreated;
                return made;
            }

            String name_;
            Array<scripting::TypeId> types_;
            engine::PPtr<engine::GameObject> created_;
        };

        RuntimeFactory& Runtime()
        {
            static RuntimeFactory* f = new RuntimeFactory();
            return *f;
        }

        ManagedFactory& Managed()
        {
            static ManagedFactory* f = new ManagedFactory();
            return *f;
        }

        IFactoryControls*& Current()
        {
            static IFactoryControls* f = nullptr;
            return f;
        }

        engine::RectTransform* RectOf(engine::GameObject* g) { return static_cast<engine::RectTransform*>(g->transform()); }

        template <class T>
        T* Get(engine::GameObject* g)
        {
            return ComponentOf<T>(g);
        }

        // Every control's object has a RectTransform first (the factory is asked for it with the components).
        engine::GameObject* Make(StringView name, std::initializer_list<scripting::TypeId> components)
        {
            scripting::TypeId rect = scripting::TypeOf<engine::RectTransform>();
            Array<scripting::TypeId> types;
            types.Push(rect);
            for (scripting::TypeId t : components)
                if (t != rect)
                    types.Push(t);
            return DefaultControls::factory()->CreateGameObject(name, Span<const scripting::TypeId>(types.Data(), types.Size()));
        }

        void SetLayerRecursively(engine::GameObject* g, u32 layer)
        {
            g->SetLayer(layer);
            engine::Transform* t = g->transform();
            for (u32 i = 0; i < t->childCount(); i++)
                SetLayerRecursively(t->GetChild(i)->gameObject(), layer);
        }

        void SetParentAndAlign(engine::GameObject* child, engine::GameObject* parent)
        {
            if (!parent)
                return;
            child->transform()->SetParent(parent->transform(), false);
            SetLayerRecursively(child, parent->layer());
        }

        engine::GameObject* CreateUIElementRoot(StringView name, Vector2 size, std::initializer_list<scripting::TypeId> components)
        {
            engine::GameObject* g = Make(name, components);
            RectOf(g)->SetSizeDelta(size);
            return g;
        }

        engine::GameObject* CreateUIObject(StringView name, engine::GameObject* parent, std::initializer_list<scripting::TypeId> components)
        {
            engine::GameObject* g = Make(name, components);
            SetParentAndAlign(g, parent);
            return g;
        }

        // (The reference reads its colors and sets them on a copy: the controls keep their color block.)
        void SetDefaultColorTransitionValues(Selectable* selectable) { (void)selectable; }

        void Sliced(Image* image, engine::Sprite* sprite, Color color)
        {
            image->SetSprite(sprite);
            image->SetType(Image::Type::Sliced);
            image->SetColor(color);
        }

        scripting::TypeId RectType() { return scripting::TypeOf<engine::RectTransform>(); }

        const Vector2 kTextElementSize(200, 50);
        const Color kTextColor(50 / 255.0f, 50 / 255.0f, 50 / 255.0f, 1);

        void SetDefaultTextValues(TextBase* text)
        {
            text->SetColor(kTextColor);
            text->SetFontSize(14);
        }

        void Stretch(engine::RectTransform* r, Vector2 offsetMin, Vector2 offsetMax)
        {
            r->SetAnchorMin(Vector2(0, 0));
            r->SetAnchorMax(Vector2(1, 1));
            r->SetSizeDelta(Vector2(0, 0));
            r->SetOffsetMin(offsetMin);
            r->SetOffsetMax(offsetMax);
        }

        // The names the two sets of controls give their objects.
        struct TextNames
        {
            StringView text, buttonText, inputField;
        };
        constexpr TextNames kDefaultNames{"Text", "Text", "InputField"};
        constexpr TextNames kTextNames{"Text (TMP)", "Text (TMP)", "InputField (TMP)"};

        CanvasText* AddText(engine::GameObject* g) { return static_cast<CanvasText*>(g->AddComponent(scripting::TypeOf<CanvasText>())); }

        template <class T>
        T* AddTo(engine::GameObject* g)
        {
            return static_cast<T*>(g->AddComponent(scripting::TypeOf<T>()));
        }

        engine::GameObject* MakeText(const TextNames& names)
        {
            engine::GameObject* root = CreateUIElementRoot(names.text, kTextElementSize, {scripting::TypeOf<CanvasText>()});
            Get<CanvasText>(root)->SetText("New Text");
            return root;
        }

        void AddButtonLabel(engine::GameObject* root, StringView name)
        {
            engine::GameObject* child = CreateUIObject(name, root, {RectType()});
            CanvasText* text = AddText(child);
            text->SetText("Button");
            text->SetAlignment(TextAlignmentOptions::Center);
            SetDefaultTextValues(text);
            Stretch(RectOf(child), Vector2(0, 0), Vector2(0, 0));
        }

        engine::GameObject* MakeInputField(const DefaultControls::Resources& resources, const TextNames& names)
        {
            engine::GameObject* root = CreateUIElementRoot(names.inputField, kThickElementSize, {RectType()});
            engine::GameObject* textArea = CreateUIObject("Text Area", root, {RectType()});
            engine::GameObject* childPlaceholder = CreateUIObject("Placeholder", textArea, {RectType()});
            engine::GameObject* childText = CreateUIObject("Text", textArea, {RectType()});

            Image* image = AddTo<Image>(root);
            Sliced(image, resources.inputField, kDefaultSelectableColor);
            TextInput* input = AddTo<TextInput>(root);
            SetDefaultColorTransitionValues(input);

            AddTo<RectMask2D>(textArea)->SetPadding(Vector4(-8, -5, -8, -5));
            engine::RectTransform* textAreaRect = RectOf(textArea);
            Stretch(textAreaRect, Vector2(10, 6), Vector2(-10, -7));

            CanvasText* text = AddText(childText);
            text->SetText("");
            text->SetEnableWordWrapping(false);
            text->SetExtraPadding(true);
            text->SetRichText(true);
            SetDefaultTextValues(text);

            CanvasText* placeholder = AddText(childPlaceholder);
            placeholder->SetText("Enter text...");
            placeholder->SetFontSize(14);
            placeholder->SetFontStyle(FontStyles::Italic);
            placeholder->SetEnableWordWrapping(false);
            placeholder->SetExtraPadding(true);
            // Half as opaque as the text.
            Color placeholderColor = text->color();
            placeholderColor.a *= 0.5f;
            placeholder->SetColor(placeholderColor);
            AddTo<LayoutElement>(childPlaceholder)->SetIgnoreLayout(true);

            Stretch(RectOf(childText), Vector2(0, 0), Vector2(0, 0));
            Stretch(RectOf(childPlaceholder), Vector2(0, 0), Vector2(0, 0));

            input->SetTextViewport(textAreaRect);
            input->SetTextComponent(text);
            input->SetPlaceholder(placeholder);
            if (text->font())
                input->SetFontAsset(text->font());
            return root;
        }

        void Place(engine::GameObject* g, Vector2 anchorMin, Vector2 anchorMax, Vector2 sizeDelta, Vector2 position)
        {
            engine::RectTransform* r = RectOf(g);
            r->SetAnchorMin(anchorMin);
            r->SetAnchorMax(anchorMax);
            r->SetSizeDelta(sizeDelta);
            r->SetAnchoredPosition(position);
        }

        engine::GameObject* MakeDropdown(const DefaultControls::Resources& resources)
        {
            engine::GameObject* root = CreateUIElementRoot("Dropdown", kThickElementSize, {RectType()});
            engine::GameObject* label = CreateUIObject("Label", root, {RectType()});
            engine::GameObject* arrow = CreateUIObject("Arrow", root, {RectType()});
            engine::GameObject* templateGo = CreateUIObject("Template", root, {RectType()});
            engine::GameObject* viewport = CreateUIObject("Viewport", templateGo, {RectType()});
            engine::GameObject* content = CreateUIObject("Content", viewport, {RectType()});
            engine::GameObject* item = CreateUIObject("Item", content, {RectType()});
            engine::GameObject* itemBackground = CreateUIObject("Item Background", item, {RectType()});
            engine::GameObject* itemCheckmark = CreateUIObject("Item Checkmark", item, {RectType()});
            engine::GameObject* itemLabel = CreateUIObject("Item Label", item, {RectType()});

            // The scrollbar of the list, up its right side.
            engine::GameObject* scrollbar = DefaultControls::CreateScrollbar(resources);
            scrollbar->SetName("Scrollbar");
            SetParentAndAlign(scrollbar, templateGo);
            Scrollbar* bar = Get<Scrollbar>(scrollbar);
            bar->SetDirection(Scrollbar::Direction::BottomToTop, true);
            engine::RectTransform* barRect = RectOf(scrollbar);
            barRect->SetAnchorMin(Vector2(1, 0));
            barRect->SetAnchorMax(Vector2(1, 1));
            barRect->SetPivot(Vector2(1, 1));
            barRect->SetSizeDelta(Vector2(barRect->sizeDelta().x, 0));

            // The item: a toggle with a background, a checkmark and a label.
            CanvasText* itemLabelText = AddText(itemLabel);
            SetDefaultTextValues(itemLabelText);
            itemLabelText->SetAlignment(TextAlignmentOptions::Left);
            Image* itemBackgroundImage = AddTo<Image>(itemBackground);
            itemBackgroundImage->SetColor(Color(245 / 255.0f, 245 / 255.0f, 245 / 255.0f, 1));
            Image* itemCheckmarkImage = AddTo<Image>(itemCheckmark);
            itemCheckmarkImage->SetSprite(resources.checkmark);
            Toggle* itemToggle = AddTo<Toggle>(item);
            itemToggle->SetTargetGraphic(itemBackgroundImage);
            itemToggle->SetGraphic(itemCheckmarkImage);
            itemToggle->SetIsOn(true);

            // The template: a scroll view over the items.
            Image* templateImage = AddTo<Image>(templateGo);
            templateImage->SetSprite(resources.standard);
            templateImage->SetType(Image::Type::Sliced);
            ScrollRect* scroll = AddTo<ScrollRect>(templateGo);
            scroll->SetContent(RectOf(content));
            scroll->SetViewport(RectOf(viewport));
            scroll->SetHorizontal(false);
            scroll->SetMovementType(ScrollRect::MovementType::Clamped);
            scroll->SetVerticalScrollbar(bar);
            scroll->SetVerticalScrollbarVisibility(ScrollRect::ScrollbarVisibility::AutoHideAndExpandViewport);
            scroll->SetVerticalScrollbarSpacing(-3);
            AddTo<Mask>(viewport)->SetShowMaskGraphic(false);
            Image* viewportImage = Get<Image>(viewport);
            if (!viewportImage)
                viewportImage = AddTo<Image>(viewport);
            viewportImage->SetSprite(resources.mask);
            viewportImage->SetType(Image::Type::Sliced);

            // The dropdown: its caption and arrow over a sliced background.
            CanvasText* labelText = AddText(label);
            SetDefaultTextValues(labelText);
            labelText->SetAlignment(TextAlignmentOptions::Left);
            AddTo<Image>(arrow)->SetSprite(resources.dropdown);
            Image* backgroundImage = AddTo<Image>(root);
            Sliced(backgroundImage, resources.standard, kDefaultSelectableColor);
            Dropdown* dropdown = AddTo<Dropdown>(root);
            dropdown->SetTargetGraphic(backgroundImage);
            SetDefaultColorTransitionValues(dropdown);
            dropdown->SetTemplateRect(RectOf(templateGo));
            dropdown->SetCaptionText(labelText);
            dropdown->SetItemText(itemLabelText);

            itemLabelText->SetText("Option A");
            Array<Dropdown::OptionData> options;
            for (StringView o : {StringView("Option A"), StringView("Option B"), StringView("Option C")})
            {
                Dropdown::OptionData d;
                d.text = String(o);
                options.Push(std::move(d));
            }
            dropdown->SetOptions(std::move(options));

            engine::RectTransform* labelRect = RectOf(label);
            labelRect->SetAnchorMin(Vector2(0, 0));
            labelRect->SetAnchorMax(Vector2(1, 1));
            labelRect->SetOffsetMin(Vector2(10, 6));
            labelRect->SetOffsetMax(Vector2(-25, -7));
            Place(arrow, Vector2(1, 0.5f), Vector2(1, 0.5f), Vector2(20, 20), Vector2(-15, 0));
            RectOf(templateGo)->SetPivot(Vector2(0.5f, 1));
            Place(templateGo, Vector2(0, 0), Vector2(1, 0), Vector2(0, 150), Vector2(0, 2));
            RectOf(viewport)->SetPivot(Vector2(0, 1));
            Place(viewport, Vector2(0, 0), Vector2(1, 1), Vector2(-18, 0), Vector2(0, 0));
            RectOf(content)->SetPivot(Vector2(0.5f, 1));
            Place(content, Vector2(0, 1), Vector2(1, 1), Vector2(0, 28), Vector2(0, 0));
            Place(item, Vector2(0, 0.5f), Vector2(1, 0.5f), Vector2(0, 20), Vector2(0, 0));
            Stretch(RectOf(itemBackground), Vector2(0, 0), Vector2(0, 0));
            Place(itemCheckmark, Vector2(0, 0.5f), Vector2(0, 0.5f), Vector2(20, 20), Vector2(10, 0));
            engine::RectTransform* itemLabelRect = RectOf(itemLabel);
            itemLabelRect->SetAnchorMin(Vector2(0, 0));
            itemLabelRect->SetAnchorMax(Vector2(1, 1));
            itemLabelRect->SetOffsetMin(Vector2(20, 1));
            itemLabelRect->SetOffsetMax(Vector2(-10, -2));
            templateGo->SetActive(false);
            return root;
        }
    }

    DefaultControls::Resources DefaultControls::Resources::From(const DefaultControlsResourcesData& d)
    {
        auto sprite = [](engine::EntityId id) { return engine::EntityIdToObject<engine::Sprite>(id); };
        Resources r;
        r.standard = sprite(d.standard);
        r.background = sprite(d.background);
        r.inputField = sprite(d.inputField);
        r.knob = sprite(d.knob);
        r.checkmark = sprite(d.checkmark);
        r.dropdown = sprite(d.dropdown);
        r.mask = sprite(d.mask);
        return r;
    }

    IFactoryControls* DefaultControls::factory() { return Current() ? Current() : &Runtime(); }

    void DefaultControls::SetFactory(IFactoryControls* value) { Current() = value; }

    engine::GameObject* DefaultControls::CreatePanel(const Resources& resources)
    {
        engine::GameObject* root = CreateUIElementRoot("Panel", kThickElementSize, {scripting::TypeOf<Image>()});
        // Stretched over its parent.
        engine::RectTransform* r = RectOf(root);
        r->SetAnchorMin(Vector2(0, 0));
        r->SetAnchorMax(Vector2(1, 1));
        r->SetAnchoredPosition(Vector2(0, 0));
        r->SetSizeDelta(Vector2(0, 0));
        Sliced(Get<Image>(root), resources.background, kPanelColor);
        return root;
    }

    engine::GameObject* DefaultControls::CreateButton(const Resources& resources)
    {
        engine::GameObject* root = CreateUIElementRoot("Button", kThickElementSize, {scripting::TypeOf<Image>(), scripting::TypeOf<Button>()});
        Sliced(Get<Image>(root), resources.standard, kDefaultSelectableColor);
        SetDefaultColorTransitionValues(Get<Button>(root));
        AddButtonLabel(root, kDefaultNames.buttonText);
        return root;
    }

    engine::GameObject* DefaultControls::CreateImage(const Resources& resources)
    {
        (void)resources;
        return CreateUIElementRoot("Image", kImageElementSize, {scripting::TypeOf<Image>()});
    }

    engine::GameObject* DefaultControls::CreateRawImage(const Resources& resources)
    {
        (void)resources;
        return CreateUIElementRoot("RawImage", kImageElementSize, {scripting::TypeOf<RawImage>()});
    }

    engine::GameObject* DefaultControls::CreateSlider(const Resources& resources)
    {
        engine::GameObject* root = CreateUIElementRoot("Slider", kThinElementSize, {scripting::TypeOf<Slider>()});
        engine::GameObject* background = CreateUIObject("Background", root, {scripting::TypeOf<Image>()});
        engine::GameObject* fillArea = CreateUIObject("Fill Area", root, {RectType()});
        engine::GameObject* fill = CreateUIObject("Fill", fillArea, {scripting::TypeOf<Image>()});
        engine::GameObject* handleArea = CreateUIObject("Handle Slide Area", root, {RectType()});
        engine::GameObject* handle = CreateUIObject("Handle", handleArea, {scripting::TypeOf<Image>()});

        Sliced(Get<Image>(background), resources.background, kDefaultSelectableColor);
        engine::RectTransform* backgroundRect = RectOf(background);
        backgroundRect->SetAnchorMin(Vector2(0, 0.25f));
        backgroundRect->SetAnchorMax(Vector2(1, 0.75f));
        backgroundRect->SetSizeDelta(Vector2(0, 0));

        engine::RectTransform* fillAreaRect = RectOf(fillArea);
        fillAreaRect->SetAnchorMin(Vector2(0, 0.25f));
        fillAreaRect->SetAnchorMax(Vector2(1, 0.75f));
        fillAreaRect->SetAnchoredPosition(Vector2(-5, 0));
        fillAreaRect->SetSizeDelta(Vector2(-20, 0));

        Sliced(Get<Image>(fill), resources.standard, kDefaultSelectableColor);
        RectOf(fill)->SetSizeDelta(Vector2(10, 0));

        engine::RectTransform* handleAreaRect = RectOf(handleArea);
        handleAreaRect->SetSizeDelta(Vector2(-20, 0));
        handleAreaRect->SetAnchorMin(Vector2(0, 0));
        handleAreaRect->SetAnchorMax(Vector2(1, 1));

        Image* handleImage = Get<Image>(handle);
        handleImage->SetSprite(resources.knob);
        handleImage->SetColor(kDefaultSelectableColor);
        RectOf(handle)->SetSizeDelta(Vector2(20, 0));

        Slider* slider = Get<Slider>(root);
        slider->SetFillRect(RectOf(fill));
        slider->SetHandleRect(RectOf(handle));
        slider->SetTargetGraphic(handleImage);
        slider->SetDirection(Slider::Direction::LeftToRight);
        SetDefaultColorTransitionValues(slider);
        return root;
    }

    engine::GameObject* DefaultControls::CreateScrollbar(const Resources& resources)
    {
        engine::GameObject* root = CreateUIElementRoot("Scrollbar", kThinElementSize, {scripting::TypeOf<Image>(), scripting::TypeOf<Scrollbar>()});
        engine::GameObject* slidingArea = CreateUIObject("Sliding Area", root, {RectType()});
        engine::GameObject* handle = CreateUIObject("Handle", slidingArea, {scripting::TypeOf<Image>()});

        Sliced(Get<Image>(root), resources.background, kDefaultSelectableColor);
        Image* handleImage = Get<Image>(handle);
        Sliced(handleImage, resources.standard, kDefaultSelectableColor);

        engine::RectTransform* areaRect = RectOf(slidingArea);
        areaRect->SetSizeDelta(Vector2(-20, -20));
        areaRect->SetAnchorMin(Vector2(0, 0));
        areaRect->SetAnchorMax(Vector2(1, 1));
        engine::RectTransform* handleRect = RectOf(handle);
        handleRect->SetSizeDelta(Vector2(20, 20));

        Scrollbar* bar = Get<Scrollbar>(root);
        bar->SetHandleRect(handleRect);
        bar->SetTargetGraphic(handleImage);
        SetDefaultColorTransitionValues(bar);
        return root;
    }

    engine::GameObject* DefaultControls::CreateToggle(const Resources& resources)
    {
        engine::GameObject* root = CreateUIElementRoot("Toggle", kThinElementSize, {scripting::TypeOf<Toggle>()});
        engine::GameObject* background = CreateUIObject("Background", root, {scripting::TypeOf<Image>()});
        engine::GameObject* checkmark = CreateUIObject("Checkmark", background, {scripting::TypeOf<Image>()});

        Toggle* toggle = Get<Toggle>(root);
        toggle->SetIsOn(true);
        Image* backgroundImage = Get<Image>(background);
        Sliced(backgroundImage, resources.standard, kDefaultSelectableColor);
        Image* checkmarkImage = Get<Image>(checkmark);
        checkmarkImage->SetSprite(resources.checkmark);
        toggle->SetGraphic(checkmarkImage);
        toggle->SetTargetGraphic(backgroundImage);
        SetDefaultColorTransitionValues(toggle);

        engine::RectTransform* backgroundRect = RectOf(background);
        backgroundRect->SetAnchorMin(Vector2(0, 1));
        backgroundRect->SetAnchorMax(Vector2(0, 1));
        backgroundRect->SetAnchoredPosition(Vector2(10, -10));
        backgroundRect->SetSizeDelta(Vector2(kThinHeight, kThinHeight));
        engine::RectTransform* checkmarkRect = RectOf(checkmark);
        checkmarkRect->SetAnchorMin(Vector2(0.5f, 0.5f));
        checkmarkRect->SetAnchorMax(Vector2(0.5f, 0.5f));
        checkmarkRect->SetAnchoredPosition(Vector2(0, 0));
        checkmarkRect->SetSizeDelta(Vector2(20, 20));

        // The label right of the box.
        engine::GameObject* label = CreateUIObject("Label", root, {RectType()});
        CanvasText* labelText = AddText(label);
        labelText->SetText("Toggle");
        SetDefaultTextValues(labelText);
        engine::RectTransform* labelRect = RectOf(label);
        labelRect->SetAnchorMin(Vector2(0, 0));
        labelRect->SetAnchorMax(Vector2(1, 1));
        labelRect->SetOffsetMin(Vector2(23, 1));
        labelRect->SetOffsetMax(Vector2(-5, -2));
        return root;
    }

    engine::GameObject* DefaultControls::CreateText(const Resources& resources)
    {
        (void)resources;
        engine::GameObject* root = MakeText(kDefaultNames);
        RectOf(root)->SetSizeDelta(kThickElementSize);
        SetDefaultTextValues(Get<CanvasText>(root));
        return root;
    }

    engine::GameObject* DefaultControls::CreateInputField(const Resources& resources) { return MakeInputField(resources, kDefaultNames); }

    engine::GameObject* DefaultControls::CreateDropdown(const Resources& resources) { return MakeDropdown(resources); }

    // ----------------------------------------------------------------------------------------- TextDefaultControls

    engine::GameObject* TextDefaultControls::CreateText(const Resources& resources)
    {
        (void)resources;
        return MakeText(kTextNames);
    }

    engine::GameObject* TextDefaultControls::CreateButton(const Resources& resources)
    {
        engine::GameObject* root = CreateUIElementRoot("Button", kThickElementSize, {scripting::TypeOf<Image>(), scripting::TypeOf<Button>()});
        Sliced(Get<Image>(root), resources.standard, kDefaultSelectableColor);
        SetDefaultColorTransitionValues(Get<Button>(root));
        AddButtonLabel(root, kTextNames.buttonText);
        return root;
    }

    engine::GameObject* TextDefaultControls::CreateInputField(const Resources& resources) { return MakeInputField(resources, kTextNames); }

    engine::GameObject* TextDefaultControls::CreateDropdown(const Resources& resources) { return MakeDropdown(resources); }

    engine::GameObject* TextDefaultControls::CreateScrollbar(const Resources& resources) { return DefaultControls::CreateScrollbar(resources); }

    engine::GameObject* DefaultControls::CreateScrollView(const Resources& resources)
    {
        engine::GameObject* root = CreateUIElementRoot("Scroll View", Vector2(200, 200), {scripting::TypeOf<Image>(), scripting::TypeOf<ScrollRect>()});
        engine::GameObject* viewport = CreateUIObject("Viewport", root, {scripting::TypeOf<Image>(), scripting::TypeOf<Mask>()});
        engine::GameObject* content = CreateUIObject("Content", viewport, {RectType()});

        // The scrollbars: along the bottom and the right side.
        engine::GameObject* hBar = CreateScrollbar(resources);
        hBar->SetName("Scrollbar Horizontal");
        SetParentAndAlign(hBar, root);
        engine::RectTransform* hRect = RectOf(hBar);
        hRect->SetAnchorMin(Vector2(0, 0));
        hRect->SetAnchorMax(Vector2(1, 0));
        hRect->SetPivot(Vector2(0, 0));
        hRect->SetSizeDelta(Vector2(0, hRect->sizeDelta().y));

        engine::GameObject* vBar = CreateScrollbar(resources);
        vBar->SetName("Scrollbar Vertical");
        SetParentAndAlign(vBar, root);
        Get<Scrollbar>(vBar)->SetDirection(Scrollbar::Direction::BottomToTop, true);
        engine::RectTransform* vRect = RectOf(vBar);
        vRect->SetAnchorMin(Vector2(1, 0));
        vRect->SetAnchorMax(Vector2(1, 1));
        vRect->SetPivot(Vector2(1, 1));
        vRect->SetSizeDelta(Vector2(vRect->sizeDelta().x, 0));

        // The viewport fills the view; the content as wide and taller (the vertical bar shows, the horizontal not).
        engine::RectTransform* viewportRect = RectOf(viewport);
        viewportRect->SetAnchorMin(Vector2(0, 0));
        viewportRect->SetAnchorMax(Vector2(1, 1));
        viewportRect->SetSizeDelta(Vector2(0, 0));
        viewportRect->SetPivot(Vector2(0, 1));
        engine::RectTransform* contentRect = RectOf(content);
        contentRect->SetAnchorMin(Vector2(0, 1));
        contentRect->SetAnchorMax(Vector2(1, 1));
        contentRect->SetSizeDelta(Vector2(0, 300));
        contentRect->SetPivot(Vector2(0, 1));

        ScrollRect* scroll = Get<ScrollRect>(root);
        scroll->SetContent(contentRect);
        scroll->SetViewport(viewportRect);
        scroll->SetHorizontalScrollbar(Get<Scrollbar>(hBar));
        scroll->SetVerticalScrollbar(Get<Scrollbar>(vBar));
        scroll->SetHorizontalScrollbarVisibility(ScrollRect::ScrollbarVisibility::AutoHideAndExpandViewport);
        scroll->SetVerticalScrollbarVisibility(ScrollRect::ScrollbarVisibility::AutoHideAndExpandViewport);
        scroll->SetHorizontalScrollbarSpacing(-3);
        scroll->SetVerticalScrollbarSpacing(-3);

        Sliced(Get<Image>(root), resources.background, kPanelColor);
        Get<Mask>(viewport)->SetShowMaskGraphic(false);
        Image* viewportImage = Get<Image>(viewport);
        viewportImage->SetSprite(resources.mask);
        viewportImage->SetType(Image::Type::Sliced);
        return root;
    }

    void DefaultControls::SetManagedFactoryInternal(bool on) { Current() = on ? &Managed() : nullptr; }

    engine::Event<>& DefaultControls::managedCreate()
    {
        static engine::Event<>* e = new engine::Event<>();
        return *e;
    }

    String DefaultControls::CreatingNameInternal() { return Managed().name_; }
    i32 DefaultControls::CreatingTypeCountInternal() { return i32(Managed().types_.Size()); }

    scripting::TypeId DefaultControls::CreatingTypeInternal(i32 index)
    {
        return index >= 0 && usize(index) < Managed().types_.Size() ? Managed().types_[usize(index)] : scripting::TypeId();
    }

    void DefaultControls::SetCreatedInternal(engine::GameObject* created) { Managed().created_ = engine::PPtr<engine::GameObject>(created); }
}
