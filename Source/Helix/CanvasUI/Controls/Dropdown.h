#pragma once

#include <Helix/CanvasUI/Controls/Selectable.h>
#include <Helix/CanvasUI/Rendering/Image.h>
#include <Helix/CanvasUI/Text/TextBase.h>
#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Script/Coroutine.h>

// A dropdown: the caption shows the option of the value (its text and image, or the placeholder at -1); a click or a
// submit shows the list (made from the template: one toggle item per option, over a blocker that closes it), picking
// an item sets the value (or, multi select, flips its bit), cancel closes it.
namespace helix::canvasui
{
    class Dropdown;
    class Toggle;

    // An option of a dropdown across the bindings (C# has OptionData over it).
    HX_STRUCT(Namespace = "HelixEngine.CanvasUI", Internal)
    struct DropdownOptionData
    {
        HX_FIELD() engine::EntityId image;
        HX_FIELD() Color color = Color(1, 1, 1, 1);
    };

    // An item of a shown list: its toggle, text and image; the pointer over it selects it.
    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class DropdownItem : public UIBehaviour, public INativeEventHandler, public IPointerEnterHandler, public ICancelHandler
    {
    public:
        HX_PROPERTY() TextBase* text() const { return static_cast<TextBase*>(text_.Get()); }
        void SetText(TextBase* value) { text_ = engine::PPtr<engine::Object>(value); }
        HX_PROPERTY() Image* image() const { return static_cast<Image*>(image_.Get()); }
        void SetImage(Image* value) { image_ = engine::PPtr<engine::Object>(value); }
        HX_PROPERTY() engine::RectTransform* rectTransform() const { return rect_.Get(); }
        void SetRectTransform(engine::RectTransform* value) { rect_ = engine::PPtr<engine::RectTransform>(value); }
        HX_PROPERTY() Toggle* toggle() const { return static_cast<Toggle*>(toggle_.Get()); }
        void SetToggle(Toggle* value) { toggle_ = engine::PPtr<engine::Object>(value); }
        HX_FUNCTION() virtual void OnPointerEnter(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnCancel(BaseEventData* eventData) override;
        bool Handles(EventKind kind) const override { return kind == EventKind::PointerEnter || kind == EventKind::Cancel; }
        void HandleEvent(EventKind kind, BaseEventData* data) override;
        INativeEventHandler* AsEventHandler() override { return this; }

    private:
        HX_FIELD(SerializeField, Name = "m_Text") engine::PPtr<engine::Object> text_;
        HX_FIELD(SerializeField, Name = "m_Image") engine::PPtr<engine::Object> image_;
        HX_FIELD(SerializeField, Name = "m_RectTransform") engine::PPtr<engine::RectTransform> rect_;
        HX_FIELD(SerializeField, Name = "m_Toggle") engine::PPtr<engine::Object> toggle_;

    public:
        // The dropdown whose list holds the item (set when the list is made).
        Dropdown* dropdownInternal() const;
        void SetDropdownInternal(Dropdown* value);

    private:
        engine::PPtr<engine::Object> dropdown_;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::RectTransform")
    class Dropdown : public Selectable, public IPointerClickHandler, public ISubmitHandler, public ICancelHandler
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        HX_STRUCT(SerializedOnly)
        struct DropdownEvent : engine::HelixEvent<i32>
        {
            static constexpr StringView kTypeName = "DropdownEvent";
        };

        struct OptionData
        {
            String text;
            engine::PPtr<engine::Object> image;
            Color color = Color(1, 1, 1, 1);
        };

        Dropdown();

        HX_PROPERTY(Name = "template") engine::RectTransform* templateRect() const { return template_.Get(); }
        void SetTemplateRect(engine::RectTransform* value);
        HX_PROPERTY() TextBase* captionText() const { return static_cast<TextBase*>(captionText_.Get()); }
        void SetCaptionText(TextBase* value);
        HX_PROPERTY() Image* captionImage() const { return static_cast<Image*>(captionImage_.Get()); }
        void SetCaptionImage(Image* value);
        HX_PROPERTY() Graphic* placeholder() const { return static_cast<Graphic*>(placeholder_.Get()); }
        void SetPlaceholder(Graphic* value);
        HX_PROPERTY() TextBase* itemText() const { return static_cast<TextBase*>(itemText_.Get()); }
        void SetItemText(TextBase* value) { itemText_ = engine::PPtr<engine::Object>(value); }
        HX_PROPERTY() Image* itemImage() const { return static_cast<Image*>(itemImage_.Get()); }
        void SetItemImage(Image* value) { itemImage_ = engine::PPtr<engine::Object>(value); }
        HX_PROPERTY() i32 value() const { return value_; }
        void SetValue(i32 value) { Set(value, true); }
        HX_FUNCTION() void SetValueWithoutNotify(i32 input) { Set(input, false); }
        HX_PROPERTY() f32 alphaFadeSpeed() const { return alphaFadeSpeed_; }
        void SetAlphaFadeSpeed(f32 value) { alphaFadeSpeed_ = value; }
        HX_PROPERTY(Name = "MultiSelect") bool multiSelect() const { return multiSelect_; }
        void SetMultiSelect(bool value) { multiSelect_ = value; }
        HX_PROPERTY(ReadOnly, Name = "IsExpanded") bool isExpanded() const { return list_.Get() != nullptr; }
        HX_EVENT(Internal) engine::Event<i32>& onValueChangedNative() { return onValueChanged_.listeners(); }
        DropdownEvent& onValueChanged() { return onValueChanged_; }

        // The options (C# keeps its OptionData list over these).
        Span<const OptionData> options() const { return Span<const OptionData>(options_.Data(), options_.Size()); }
        void SetOptions(Array<OptionData> options);
        void AddOptions(Span<const OptionData> options);
        HX_FUNCTION(Internal, Name = "ClearOptionsNative") void ClearOptions();
        HX_FUNCTION(Internal) i32 OptionCountInternal() const { return i32(options_.Size()); }
        HX_FUNCTION(Internal) String OptionTextInternal(i32 index) const;
        HX_FUNCTION(Internal) DropdownOptionData OptionDataInternal(i32 index) const;
        HX_FUNCTION(Internal) void ClearOptionsInternal() { options_.Clear(); }
        HX_FUNCTION(Internal) void AddOptionInternal(StringView text, DropdownOptionData data);
        HX_FUNCTION() void RefreshShownValue();
        // C#'s options list, pushed in before the native side reads the options.
        HX_EVENT(Internal) static engine::Event<Dropdown*>& managedOptions();
        HX_FUNCTION(Internal) void SetManagedOptionsInternal(bool value) { managedOptions_ = value; }

        HX_FUNCTION() void Show();
        HX_FUNCTION() void Hide();
        HX_FUNCTION() virtual void OnPointerClick(PointerEventData* eventData) override;
        HX_FUNCTION() virtual void OnSubmit(BaseEventData* eventData) override;
        HX_FUNCTION() virtual void OnCancel(BaseEventData* eventData) override;
        // An item picked (the list closes, the value or its bit set).
        void OnSelectItem(DropdownItem* item);

        void Awake() override;
        void Start() override;
        void OnDisable() override;

    protected:
        bool HandlesMore(EventKind kind) const override
        {
            return kind == EventKind::PointerClick || kind == EventKind::Submit || kind == EventKind::Cancel;
        }
        void HandleMore(EventKind kind, BaseEventData* data) override;

    private:
        void Set(i32 value, bool sendCallback);
        void SetupTemplate();
        DropdownItem* AddItem(const OptionData& data, DropdownItem* itemTemplate);
        engine::GameObject* CreateBlocker(engine::Canvas* rootCanvas);
        void CopyRaycasters(engine::GameObject* target) const;
        void AlphaFadeList(f32 duration, f32 start, f32 end);
        void SetAlpha(f32 alpha);
        void ImmediateDestroyDropdownList();
        engine::Coroutine FadeRoutine(f32 duration, f32 start, f32 end);
        engine::Coroutine DelayedDestroyRoutine(f32 delay);
        static void OnItemValue(void* user, bool value);
        static void OnBlockerClick(void* user);

        HX_FIELD(SerializeField, Name = "m_Template") engine::PPtr<engine::RectTransform> template_;
        HX_FIELD(SerializeField, Name = "m_CaptionText") engine::PPtr<engine::Object> captionText_;
        HX_FIELD(SerializeField, Name = "m_CaptionImage") engine::PPtr<engine::Object> captionImage_;
        HX_FIELD(SerializeField, Name = "m_Placeholder") engine::PPtr<engine::Object> placeholder_;
        HX_FIELD(SerializeField, Name = "m_ItemText") engine::PPtr<engine::Object> itemText_;
        HX_FIELD(SerializeField, Name = "m_ItemImage") engine::PPtr<engine::Object> itemImage_;
        HX_FIELD(SerializeField, Name = "m_Value") i32 value_ = 0;
        HX_FIELD(SerializeField, Name = "m_MultiSelect") bool multiSelect_ = false;
        HX_FIELD(SerializeField, Name = "m_AlphaFadeSpeed") f32 alphaFadeSpeed_ = 0.15f;
        HX_FIELD(SerializeField, Name = "m_OnValueChanged") DropdownEvent onValueChanged_;
        Array<OptionData> options_;
        engine::PPtr<engine::GameObject> list_, blocker_;
        Array<engine::PPtr<engine::Object>> items_;
        engine::CoroutineRef fade_, destroy_;
        bool validTemplate_ = false, managedOptions_ = false;
        void SyncOptions();
    };
}
