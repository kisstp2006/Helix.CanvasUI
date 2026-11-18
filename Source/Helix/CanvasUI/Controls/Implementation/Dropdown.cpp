#include <Helix/CanvasUI/Controls/Dropdown.h>

#include <Helix/CanvasUI/Events/EventSystem.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Engine/Transform/RectTransform.h>
#include <Helix/Engine/Transform/RectTransformUtility.h>

namespace helix::canvasui
{
    namespace
    {
        // The rows of a multi select list before the options, and the captions of its values.
        const Dropdown::OptionData& NothingOption()
        {
            static Dropdown::OptionData* o = new Dropdown::OptionData{String("Nothing"), {}, Color(1, 1, 1, 1)};
            return *o;
        }

        const Dropdown::OptionData& EverythingOption()
        {
            static Dropdown::OptionData* o = new Dropdown::OptionData{String("Everything"), {}, Color(1, 1, 1, 1)};
            return *o;
        }

        const Dropdown::OptionData& MixedOption()
        {
            static Dropdown::OptionData* o = new Dropdown::OptionData{String("Mixed..."), {}, Color(1, 1, 1, 1)};
            return *o;
        }

        const Dropdown::OptionData& NoOption()
        {
            static Dropdown::OptionData* o = new Dropdown::OptionData();
            return *o;
        }

        bool IsEverythingValue(i32 count, i32 value)
        {
            for (i32 i = 0; i < count && i < 31; i++)
                if ((value & (1 << i)) == 0)
                    return false;
            return true;
        }

        i32 EverythingValue(i32 count)
        {
            i32 v = 0;
            for (i32 i = 0; i < count && i < 31; i++)
                v |= 1 << i;
            return v;
        }

        i32 FirstActiveFlagIndex(i32 value)
        {
            for (i32 i = 0; i < 32; i++)
                if ((value & (1 << i)) != 0)
                    return i;
            return 0;
        }

        bool IsPowerOfTwo(i32 v) { return v > 0 && (v & (v - 1)) == 0; }

        // The component of the class on the GameObject, added when missing (script classes by their type).
        template <class T>
        T* GetOrAdd(engine::GameObject* g)
        {
            if (T* c = ComponentOf<T>(g))
                return c;
            if constexpr (std::is_base_of_v<engine::GameBehaviour, T>)
                return static_cast<T*>(g->AddComponent(scripting::TypeOf<T>()));
            else
                return g->AddComponent<T>();
        }

        engine::RectTransform* RectOf(engine::Component* c) { return static_cast<engine::RectTransform*>(c->transform()); }

        bool IsUnder(engine::Transform* t, engine::Transform* root)
        {
            for (; t; t = t->parent())
                if (t == root)
                    return true;
            return false;
        }
    }

    // -------------------------------------------------------------------------------------------------- DropdownItem

    void DropdownItem::SetDropdownInternal(Dropdown* value) { dropdown_ = engine::PPtr<engine::Object>(value); }

    Dropdown* DropdownItem::dropdownInternal() const { return static_cast<Dropdown*>(dropdown_.Get()); }

    void DropdownItem::OnPointerEnter(PointerEventData* eventData)
    {
        (void)eventData;
        if (EventSystem* es = EventSystem::current())
            es->SetSelectedGameObject(gameObject());
    }

    void DropdownItem::OnCancel(BaseEventData* eventData)
    {
        (void)eventData;
        Dropdown* d = dropdownInternal();
        if (!d)
            d = static_cast<Dropdown*>(gameObject()->GetComponentInParent(scripting::TypeOf<Dropdown>()));
        if (d)
            d->Hide();
    }

    void DropdownItem::HandleEvent(EventKind kind, BaseEventData* data)
    {
        if (kind == EventKind::PointerEnter)
            OnPointerEnter(static_cast<PointerEventData*>(data));
        else if (kind == EventKind::Cancel)
            OnCancel(data);
    }

    // ------------------------------------------------------------------------------------------------------ Dropdown

    Dropdown::Dropdown()
    {
        static bool registered = false;
        if (!registered)
        {
            registered = true;
            engine::NativeEventEntry e;
            e.storage = [](engine::Object* o) -> engine::HelixEventStorage* { return &static_cast<Dropdown*>(o)->onValueChanged_; };
            e.raise = [](engine::Object* o, Span<const scripting::Variant> a) {
                i32 v = 0;
                if (!a.Empty())
                    a[0].Get(v);
                static_cast<Dropdown*>(o)->onValueChanged_.Invoke(v);
            };
            engine::RegisterNativeEvent(scripting::TypeOf<Dropdown>(), 0, e);
        }
    }

    engine::Event<Dropdown*>& Dropdown::managedOptions()
    {
        static engine::Event<Dropdown*>* e = new engine::Event<Dropdown*>();
        return *e;
    }

    void Dropdown::SyncOptions()
    {
        if (managedOptions_)
            managedOptions().Invoke(this);
    }

    void Dropdown::SetTemplateRect(engine::RectTransform* value)
    {
        template_ = engine::PPtr<engine::RectTransform>(value);
        validTemplate_ = false;
    }

    void Dropdown::SetCaptionText(TextBase* value)
    {
        captionText_ = engine::PPtr<engine::Object>(value);
        RefreshShownValue();
    }

    void Dropdown::SetCaptionImage(Image* value)
    {
        captionImage_ = engine::PPtr<engine::Object>(value);
        RefreshShownValue();
    }

    void Dropdown::SetPlaceholder(Graphic* value)
    {
        placeholder_ = engine::PPtr<engine::Object>(value);
        RefreshShownValue();
    }

    void Dropdown::Awake()
    {
        Selectable::Awake();
        if (Image* image = captionImage())
            image->SetEnabled(image->sprite() != nullptr && image->color().a > 0);
        if (engine::RectTransform* t = templateRect())
            t->gameObject()->SetActive(false);
    }

    void Dropdown::Start()
    {
        Selectable::Start();
        RefreshShownValue();
    }

    void Dropdown::OnDisable()
    {
        ImmediateDestroyDropdownList();
        if (engine::GameObject* b = blocker_.Get())
            engine::Object::Destroy(b);
        blocker_ = engine::PPtr<engine::GameObject>();
        Selectable::OnDisable();
    }

    void Dropdown::SetOptions(Array<OptionData> options)
    {
        options_ = std::move(options);
        RefreshShownValue();
    }

    void Dropdown::AddOptions(Span<const OptionData> options)
    {
        for (const OptionData& o : options)
            options_.Push(o);
        RefreshShownValue();
    }

    void Dropdown::ClearOptions()
    {
        options_.Clear();
        value_ = placeholder() ? -1 : 0;
        RefreshShownValue();
    }

    String Dropdown::OptionTextInternal(i32 index) const
    {
        return index >= 0 && usize(index) < options_.Size() ? options_[usize(index)].text : String();
    }

    DropdownOptionData Dropdown::OptionDataInternal(i32 index) const
    {
        DropdownOptionData d;
        if (index >= 0 && usize(index) < options_.Size())
        {
            d.image = options_[usize(index)].image.Id();
            d.color = options_[usize(index)].color;
        }
        return d;
    }

    void Dropdown::AddOptionInternal(StringView text, DropdownOptionData data)
    {
        OptionData o;
        o.text = String(text);
        o.image = engine::PPtr<engine::Object>(engine::EntityIdToObject<engine::Object>(data.image));
        o.color = data.color;
        options_.Push(std::move(o));
    }

    void Dropdown::Set(i32 value, bool sendCallback)
    {
        SyncOptions();
        if (value == value_ || options_.Empty())
            return;
        i32 count = i32(options_.Size());
        value_ = multiSelect_ ? value : mathf::Clamp(value, placeholder() ? -1 : 0, count - 1);
        RefreshShownValue();
        if (sendCallback)
            onValueChanged_.Invoke(value_);
    }

    void Dropdown::RefreshShownValue()
    {
        SyncOptions();
        const OptionData* data = &NoOption();
        i32 count = i32(options_.Size());
        if (count > 0)
        {
            if (multiSelect_)
            {
                i32 first = FirstActiveFlagIndex(value_);
                if (value_ == 0 || first >= count)
                    data = &NothingOption();
                else if (IsEverythingValue(count, value_))
                    data = &EverythingOption();
                else if (IsPowerOfTwo(value_))
                    data = &options_[usize(first)];
                else
                    data = &MixedOption();
            }
            else if (value_ >= 0)
                data = &options_[usize(mathf::Clamp(value_, 0, count - 1))];
        }
        if (TextBase* caption = captionText())
            caption->SetText(data->text);
        if (Image* image = captionImage())
        {
            image->SetSprite(static_cast<engine::Sprite*>(data->image.Get()));
            image->SetColor(data->color);
            image->SetEnabled(image->sprite() != nullptr && image->color().a > 0);
        }
        if (Graphic* p = placeholder())
            p->SetEnabled(count == 0 || value_ == -1);
    }

    void Dropdown::CopyRaycasters(engine::GameObject* target) const
    {
        // The raycasters of the canvas the template sits under (the popup answers rays like it), else a graphic one.
        engine::Canvas* parentCanvas = nullptr;
        if (engine::RectTransform* t = templateRect())
            for (engine::Transform* p = t->parent(); p && !parentCanvas; p = p->parent())
                parentCanvas = p->gameObject()->GetComponent<engine::Canvas>();
        if (!parentCanvas)
        {
            GetOrAdd<GraphicRaycaster>(target);
            return;
        }
        Array<engine::Component*> raycasters = parentCanvas->gameObject()->GetComponents(scripting::TypeOf<BaseRaycaster>());
        for (engine::Component* r : raycasters)
        {
            scripting::TypeId type = r->Class().type;
            if (!target->GetComponent(type))
                target->AddComponent(type);
        }
    }

    void Dropdown::SetupTemplate()
    {
        validTemplate_ = false;
        engine::RectTransform* t = templateRect();
        if (!t)
            return;
        engine::GameObject* templateGo = t->gameObject();
        templateGo->SetActive(true);
        auto* itemToggle = static_cast<Toggle*>(templateGo->GetComponentInChildren(scripting::TypeOf<Toggle>()));
        bool valid = itemToggle && itemToggle->transform() != t && itemToggle->transform()->parent() &&
                     itemToggle->transform()->parent()->gameObject()->GetComponent<engine::RectTransform>() != nullptr;
        if (valid && itemText())
            valid = IsUnder(itemText()->transform(), itemToggle->transform());
        if (valid && itemImage())
            valid = IsUnder(itemImage()->transform(), itemToggle->transform());
        if (!valid)
        {
            templateGo->SetActive(false);
            return;
        }

        DropdownItem* item = GetOrAdd<DropdownItem>(itemToggle->gameObject());
        item->SetText(itemText());
        item->SetImage(itemImage());
        item->SetToggle(itemToggle);
        item->SetRectTransform(RectOf(itemToggle));

        // Its own canvas over the others, the raycasters of the parent canvas, and a group for the fade.
        engine::Canvas* popupCanvas = GetOrAdd<engine::Canvas>(templateGo);
        popupCanvas->SetOverrideSorting(true);
        popupCanvas->SetSortingOrder(30000);
        CopyRaycasters(templateGo);
        GetOrAdd<engine::CanvasGroup>(templateGo);
        templateGo->SetActive(false);
        validTemplate_ = true;
    }

    DropdownItem* Dropdown::AddItem(const OptionData& data, DropdownItem* itemTemplate)
    {
        engine::Transform* parent = itemTemplate->rectTransform()->parent();
        auto* item = static_cast<DropdownItem*>(engine::Object::Instantiate(itemTemplate, parent));
        if (!item)
            return nullptr;
        item->gameObject()->SetActive(true);
        String name = data.text.Empty() ? Format("Item {}", items_.Size()) : Format("Item {}: {}", items_.Size(), data.text);
        item->gameObject()->SetName(name);
        item->SetDropdownInternal(this);
        if (Toggle* toggle = item->toggle())
            toggle->SetIsOnWithoutNotify(false);
        if (TextBase* text = item->text())
            text->SetText(data.text);
        if (Image* image = item->image())
        {
            image->SetSprite(static_cast<engine::Sprite*>(data.image.Get()));
            image->SetColor(data.color);
            image->SetEnabled(image->sprite() != nullptr && data.color.a > 0);
        }
        items_.Push(engine::PPtr<engine::Object>(item));
        return item;
    }

    void Dropdown::OnItemValue(void* user, bool value)
    {
        (void)value;
        auto* item = static_cast<DropdownItem*>(user);
        if (Dropdown* d = item->dropdownInternal())
            d->OnSelectItem(item);
    }

    void Dropdown::OnBlockerClick(void* user) { static_cast<Dropdown*>(user)->Hide(); }

    void Dropdown::Show()
    {
        if (destroy_)
        {
            StopCoroutine(destroy_);
            ImmediateDestroyDropdownList();
        }
        if (!IsActive() || !IsInteractable() || list_.Get())
            return;
        SyncOptions();

        // The canvas the list sorts in: the nearest above that is a root or overrides its sorting.
        Array<engine::Component*> canvases;
        gameObject()->GetComponentsInParent(engine::ClassOf<engine::Canvas>(), false, canvases);
        if (canvases.Empty())
            return;
        auto* rootCanvas = static_cast<engine::Canvas*>(canvases[canvases.Size() - 1]);
        for (engine::Component* c : canvases)
        {
            auto* canvas = static_cast<engine::Canvas*>(c);
            if (canvas->isRootCanvas() || canvas->overrideSorting())
            {
                rootCanvas = canvas;
                break;
            }
        }

        if (!validTemplate_)
        {
            SetupTemplate();
            if (!validTemplate_)
                return;
        }

        engine::RectTransform* t = templateRect();
        t->gameObject()->SetActive(true);
        t->gameObject()->GetComponent<engine::Canvas>()->SetSortingLayerID(rootCanvas->sortingLayerID());

        auto* listGo = static_cast<engine::GameObject*>(engine::Object::Instantiate(t->gameObject(), t->parent()));
        list_ = engine::PPtr<engine::GameObject>(listGo);
        listGo->SetName("Dropdown List");
        listGo->SetActive(true);
        auto* listRect = static_cast<engine::RectTransform*>(listGo->transform());

        auto* itemTemplate = static_cast<DropdownItem*>(listGo->GetComponentInChildren(scripting::TypeOf<DropdownItem>()));
        engine::RectTransform* itemTemplateRect = itemTemplate->rectTransform();
        auto* contentRect = static_cast<engine::RectTransform*>(itemTemplateRect->parent());
        itemTemplateRect->gameObject()->SetActive(true);

        // The item's margins inside the content.
        Rect contentArea = contentRect->rect(), itemArea = itemTemplateRect->rect();
        Vector3 lp = itemTemplateRect->localPosition();
        Vector2 offsetMin = itemArea.min() - contentArea.min() + Vector2(lp.x, lp.y);
        Vector2 offsetMax = itemArea.max() - contentArea.max() + Vector2(lp.x, lp.y);
        Vector2 itemSize = itemArea.size();

        items_.Clear();
        Toggle* prev = nullptr;
        i32 count = i32(options_.Size());
        auto link = [&](DropdownItem* item, bool on) {
            Toggle* toggle = item->toggle();
            if (!toggle)
                return;
            toggle->SetIsOnWithoutNotify(on);
            toggle->onValueChanged().listeners().Add(&Dropdown::OnItemValue, item);
            if (on)
                toggle->Select();
            if (prev)
            {
                // Explicit navigation down the list.
                NavigationData prevNav = prev->NavigationOf(), nav = toggle->NavigationOf();
                prevNav.mode = NavigationMode::Explicit, nav.mode = NavigationMode::Explicit;
                prevNav.selectOnDown = toggle->GetEntityId(), prevNav.selectOnRight = toggle->GetEntityId();
                nav.selectOnLeft = prev->GetEntityId(), nav.selectOnUp = prev->GetEntityId();
                prev->SetNavigationOf(prevNav);
                toggle->SetNavigationOf(nav);
            }
            prev = toggle;
        };
        if (multiSelect_ && count > 0)
        {
            for (const OptionData* special : {&NothingOption(), &EverythingOption()})
            {
                DropdownItem* item = AddItem(*special, itemTemplate);
                if (!item)
                    continue;
                if (Image* image = item->image())
                    image->gameObject()->SetActive(false);
                link(item, special == &NothingOption() ? value_ == 0 : IsEverythingValue(count, value_));
            }
        }
        for (i32 i = 0; i < count; i++)
        {
            DropdownItem* item = AddItem(options_[usize(i)], itemTemplate);
            if (!item)
                continue;
            link(item, multiSelect_ ? (value_ & (1 << i)) != 0 : value_ == i);
        }

        // The content as tall as the items; the list no taller than the content.
        Vector2 sizeDelta = contentRect->sizeDelta();
        sizeDelta.y = itemSize.y * f32(items_.Size()) + offsetMin.y - offsetMax.y;
        contentRect->SetSizeDelta(sizeDelta);
        f32 extraSpace = listRect->rect().height - contentRect->rect().height;
        if (extraSpace > 0)
            listRect->SetSizeDelta(Vector2(listRect->sizeDelta().x, listRect->sizeDelta().y - extraSpace));

        // Flipped on an axis it leaves the root canvas on.
        Vector3 corners[4];
        listRect->GetWorldCorners(corners);
        auto* rootRect = static_cast<engine::RectTransform*>(rootCanvas->transform());
        Rect rootArea = rootRect->rect();
        for (i32 axis = 0; axis < 2; axis++)
        {
            bool outside = false;
            for (const Vector3& c : corners)
            {
                Vector3 corner = rootRect->InverseTransformPoint(c);
                f32 v = axis == 0 ? corner.x : corner.y;
                f32 lo = axis == 0 ? rootArea.min().x : rootArea.min().y;
                f32 hi = axis == 0 ? rootArea.max().x : rootArea.max().y;
                if ((v < lo && !mathf::Approximately(v, lo)) || (v > hi && !mathf::Approximately(v, hi)))
                {
                    outside = true;
                    break;
                }
            }
            if (outside)
                engine::RectTransformUtility::FlipLayoutOnAxis(listRect, axis, false, false);
        }

        // The items stacked from the top.
        usize n = items_.Size();
        for (usize i = 0; i < n; i++)
        {
            engine::RectTransform* r = static_cast<DropdownItem*>(items_[i].Get())->rectTransform();
            r->SetAnchorMin(Vector2(r->anchorMin().x, 0));
            r->SetAnchorMax(Vector2(r->anchorMax().x, 0));
            r->SetAnchoredPosition(Vector2(r->anchoredPosition().x, offsetMin.y + itemSize.y * f32(n - 1 - i) + itemSize.y * r->pivot().y));
            r->SetSizeDelta(Vector2(r->sizeDelta().x, itemSize.y));
        }

        AlphaFadeList(alphaFadeSpeed_, 0, 1);
        t->gameObject()->SetActive(false);
        itemTemplate->gameObject()->SetActive(false);
        blocker_ = engine::PPtr<engine::GameObject>(CreateBlocker(rootCanvas));
    }

    engine::GameObject* Dropdown::CreateBlocker(engine::Canvas* rootCanvas)
    {
        // A clear button over the root canvas, just under the list: a click anywhere else closes it.
        engine::GameObject* blocker = engine::GameObject::Create("Blocker");
        blocker->SetLayer(rootCanvas->gameObject()->layer());
        auto* rect = blocker->AddComponent<engine::RectTransform>();
        rect = static_cast<engine::RectTransform*>(blocker->transform());
        rect->SetParent(rootCanvas->transform(), false);
        rect->SetAnchorMin(Vector2(0, 0));
        rect->SetAnchorMax(Vector2(1, 1));
        rect->SetSizeDelta(Vector2(0, 0));

        auto* canvas = blocker->AddComponent<engine::Canvas>();
        canvas->SetOverrideSorting(true);
        auto* listCanvas = list_.Get()->GetComponent<engine::Canvas>();
        canvas->SetSortingLayerID(listCanvas->sortingLayerID());
        canvas->SetSortingOrder(listCanvas->sortingOrder() - 1);
        CopyRaycasters(blocker);

        auto* image = static_cast<Image*>(blocker->AddComponent(scripting::TypeOf<Image>()));
        image->SetColor(Color(0, 0, 0, 0));
        auto* button = static_cast<Button*>(blocker->AddComponent(scripting::TypeOf<Button>()));
        button->onClick().listeners().Add(&Dropdown::OnBlockerClick, this);
        blocker->AddComponent<engine::CanvasGroup>()->SetIgnoreParentGroups(true);
        return blocker;
    }

    void Dropdown::SetAlpha(f32 alpha)
    {
        if (engine::GameObject* l = list_.Get())
            if (auto* group = l->GetComponent<engine::CanvasGroup>())
                group->SetAlpha(alpha);
    }

    engine::Coroutine Dropdown::FadeRoutine(f32 duration, f32 start, f32 end)
    {
        f32 elapsed = 0;
        while (elapsed < duration)
        {
            co_await engine::WaitForEndOfFrame();
            elapsed += engine::Time::unscaledDeltaTime();
            SetAlpha(mathf::Lerp(start, end, mathf::Clamp01(elapsed / duration)));
        }
    }

    void Dropdown::AlphaFadeList(f32 duration, f32 start, f32 end)
    {
        if (end == start)
            return;
        StopCoroutine(fade_);
        fade_ = engine::CoroutineRef();
        SetAlpha(start);
        if (duration <= 0 || !IsActive())
        {
            SetAlpha(end);
            return;
        }
        fade_ = StartCoroutine(FadeRoutine(duration, start, end));
    }

    engine::Coroutine Dropdown::DelayedDestroyRoutine(f32 delay)
    {
        co_await engine::WaitForSecondsRealtime(delay);
        destroy_ = engine::CoroutineRef();
        ImmediateDestroyDropdownList();
    }

    void Dropdown::ImmediateDestroyDropdownList()
    {
        for (auto& item : items_)
            if (auto* i = static_cast<DropdownItem*>(item.Get()))
                if (Toggle* toggle = i->toggle())
                    toggle->onValueChanged().listeners().Remove(&Dropdown::OnItemValue, i);
        items_.Clear();
        if (engine::GameObject* l = list_.Get())
            engine::Object::Destroy(l);
        StopCoroutine(fade_);
        fade_ = engine::CoroutineRef();
        StopCoroutine(destroy_);
        destroy_ = engine::CoroutineRef();
        list_ = engine::PPtr<engine::GameObject>();
    }

    void Dropdown::Hide()
    {
        if (destroy_)
            return;
        if (list_.Get())
        {
            f32 alpha = 1;
            if (auto* group = list_.Get()->GetComponent<engine::CanvasGroup>())
                alpha = group->alpha();
            AlphaFadeList(alphaFadeSpeed_, alpha, 0);
            if (IsActive())
                destroy_ = StartCoroutine(DelayedDestroyRoutine(alphaFadeSpeed_));
            if (!destroy_)
                ImmediateDestroyDropdownList();
        }
        if (engine::GameObject* b = blocker_.Get())
            engine::Object::Destroy(b);
        blocker_ = engine::PPtr<engine::GameObject>();
        Select();
    }

    void Dropdown::OnSelectItem(DropdownItem* item)
    {
        Toggle* toggle = item ? item->toggle() : nullptr;
        if (!toggle)
            return;
        // The row under the content (its first child is the inactive item template).
        engine::Transform* tr = toggle->transform();
        engine::Transform* parent = tr->parent();
        i32 selected = -1;
        for (u32 i = 1; parent && i < parent->childCount(); i++)
            if (parent->GetChild(i) == tr)
            {
                selected = i32(i) - 1;
                break;
            }
        if (selected < 0)
            return;

        if (multiSelect_)
        {
            auto rowToggle = [&](u32 child) {
                return static_cast<Toggle*>(parent->GetChild(child)->gameObject()->GetComponentInChildren(scripting::TypeOf<Toggle>()));
            };
            i32 count = i32(options_.Size());
            if (selected == 0)
            {
                SetValue(0);
                for (u32 i = 2; i < parent->childCount(); i++)
                    if (Toggle* t = rowToggle(i))
                        t->SetIsOnWithoutNotify(false);
                toggle->SetIsOnWithoutNotify(true);
            }
            else if (selected == 1)
            {
                SetValue(EverythingValue(count));
                for (u32 i = 3; i < parent->childCount(); i++)
                    if (Toggle* t = rowToggle(i))
                        t->SetIsOnWithoutNotify(true);
                if (Toggle* nothing = rowToggle(1))
                    nothing->SetIsOnWithoutNotify(false);
                toggle->SetIsOnWithoutNotify(true);
            }
            else
            {
                i32 flag = 1 << (selected - 2);
                bool wasSelected = (value_ & flag) != 0;
                toggle->SetIsOnWithoutNotify(!wasSelected);
                SetValue(wasSelected ? value_ & ~flag : value_ | flag);
                if (Toggle* nothing = rowToggle(1))
                    nothing->SetIsOnWithoutNotify(value_ == 0);
                if (Toggle* everything = rowToggle(2))
                    everything->SetIsOnWithoutNotify(IsEverythingValue(count, value_));
            }
            return;
        }
        if (!toggle->isOn())
            toggle->SetIsOnWithoutNotify(true);
        SetValue(selected);
        Hide();
    }

    void Dropdown::OnPointerClick(PointerEventData* eventData)
    {
        (void)eventData;
        Show();
    }

    void Dropdown::OnSubmit(BaseEventData* eventData)
    {
        (void)eventData;
        Show();
    }

    void Dropdown::OnCancel(BaseEventData* eventData)
    {
        (void)eventData;
        Hide();
    }

    void Dropdown::HandleMore(EventKind kind, BaseEventData* data)
    {
        switch (kind)
        {
        case EventKind::PointerClick: OnPointerClick(static_cast<PointerEventData*>(data)); break;
        case EventKind::Submit: OnSubmit(data); break;
        case EventKind::Cancel: OnCancel(data); break;
        default: break;
        }
    }

    void Dropdown::OnAfterCloneNative()
    {
        Selectable::OnAfterCloneNative();
        list_ = engine::PPtr<engine::GameObject>();
        blocker_ = engine::PPtr<engine::GameObject>();
        items_.Clear();
        fade_ = engine::CoroutineRef();
        destroy_ = engine::CoroutineRef();
        validTemplate_ = false;
    }
}
