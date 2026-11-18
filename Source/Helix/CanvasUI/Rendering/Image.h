#pragma once

#include <Helix/CanvasUI/Layout/LayoutRebuilder.h>
#include <Helix/CanvasUI/Rendering/Graphic.h>
#include <Helix/Engine/Sprite/Sprite.h>

// Image draws a sprite: as it is (Simple, also through the sprite's own mesh), as nine slices by its borders
// (Sliced), repeated (Tiled) or partly by a fill amount (Filled: horizontally, vertically or radially around a corner,
// an edge or the center). It is a layout element of the sprite's size, and takes raycasts where the sprite's alpha
// reaches a threshold. RawImage draws a texture through a uv rectangle.
namespace helix::canvasui
{
    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class Image : public MaskableGraphic, public ILayoutElement, public ICanvasRaycastFilterNative
    {
    public:
        HX_ENUM()
        enum class Type : i32
        {
            Simple = 0,
            Sliced = 1,
            Tiled = 2,
            Filled = 3,
        };
        HX_ENUM()
        enum class FillMethod : i32
        {
            Horizontal = 0,
            Vertical = 1,
            Radial90 = 2,
            Radial180 = 3,
            Radial360 = 4,
        };
        HX_ENUM()
        enum class OriginHorizontal : i32
        {
            Left = 0,
            Right = 1,
        };
        HX_ENUM()
        enum class OriginVertical : i32
        {
            Bottom = 0,
            Top = 1,
        };
        HX_ENUM()
        enum class Origin90 : i32
        {
            BottomLeft = 0,
            TopLeft = 1,
            TopRight = 2,
            BottomRight = 3,
        };
        HX_ENUM()
        enum class Origin180 : i32
        {
            Bottom = 0,
            Left = 1,
            Top = 2,
            Right = 3,
        };
        HX_ENUM()
        enum class Origin360 : i32
        {
            Bottom = 0,
            Right = 1,
            Top = 2,
            Left = 3,
        };

        Image() = default;
        ~Image() override = default;

        HX_PROPERTY(ReadOnly) static engine::Material* defaultETC1GraphicMaterial();

        HX_PROPERTY() engine::Sprite* sprite() const { return sprite_.Get(); }
        void SetSprite(engine::Sprite* value);
        HX_PROPERTY() engine::Sprite* overrideSprite() const { return activeSprite(); }
        void SetOverrideSprite(engine::Sprite* value);
        HX_PROPERTY() Type type() const { return type_; }
        void SetType(Type value);
        HX_PROPERTY() bool preserveAspect() const { return preserveAspect_; }
        void SetPreserveAspect(bool value);
        HX_PROPERTY() bool fillCenter() const { return fillCenter_; }
        void SetFillCenter(bool value);
        HX_PROPERTY() FillMethod fillMethod() const { return fillMethod_; }
        void SetFillMethod(FillMethod value);
        HX_PROPERTY() f32 fillAmount() const { return fillAmount_; }
        void SetFillAmount(f32 value);
        HX_PROPERTY() bool fillClockwise() const { return fillClockwise_; }
        void SetFillClockwise(bool value);
        HX_PROPERTY() i32 fillOrigin() const { return fillOrigin_; }
        void SetFillOrigin(i32 value);
        HX_PROPERTY(Attributes = "[System.Obsolete(\"eventAlphaThreshold has been deprecated. Use eventMinimumAlphaThreshold instead (UnityUpgradable) -> alphaHitTestMinimumThreshold\")]")
        f32 eventAlphaThreshold() const { return 1 - alphaHitTestMinimumThreshold_; }
        void SetEventAlphaThreshold(f32 value) { SetAlphaHitTestMinimumThreshold(1 - value); }
        HX_PROPERTY() f32 alphaHitTestMinimumThreshold() const { return alphaHitTestMinimumThreshold_; }
        void SetAlphaHitTestMinimumThreshold(f32 value);
        HX_PROPERTY() bool useSpriteMesh() const { return useSpriteMesh_; }
        void SetUseSpriteMesh(bool value);
        HX_PROPERTY(ReadOnly) bool hasBorder() const;
        HX_PROPERTY() f32 pixelsPerUnitMultiplier() const { return pixelsPerUnitMultiplier_; }
        void SetPixelsPerUnitMultiplier(f32 value);
        HX_PROPERTY(ReadOnly) f32 pixelsPerUnit();
        HX_PROPERTY(ReadOnly, Protected) f32 multipliedPixelsPerUnit() { return pixelsPerUnit() * pixelsPerUnitMultiplier_; }
        HX_FUNCTION() void DisableSpriteOptimizations() {}
        HX_FUNCTION() virtual void OnBeforeSerialize() {}
        HX_FUNCTION() virtual void OnAfterDeserialize();

        engine::Texture* mainTexture() const override;
        engine::Material* material() const override;
        void SetNativeSize() override;
        void OnPopulateMesh(VertexHelper* toFill) override;
        void UpdateMaterial() override;

        // ILayoutElement
        HX_FUNCTION() void CalculateLayoutInputHorizontal() override {}
        HX_FUNCTION() void CalculateLayoutInputVertical() override {}
        HX_PROPERTY() f32 minWidth() const override { return 0; }
        HX_PROPERTY() f32 preferredWidth() const override;
        HX_PROPERTY() f32 flexibleWidth() const override { return -1; }
        HX_PROPERTY() f32 minHeight() const override { return 0; }
        HX_PROPERTY() f32 preferredHeight() const override;
        HX_PROPERTY() f32 flexibleHeight() const override { return -1; }
        HX_PROPERTY() i32 layoutPriority() const override { return 0; }
        HX_FUNCTION() bool IsRaycastLocationValid(Vector2 screenPoint, engine::Camera* eventCamera) override;

        ILayoutElement* AsLayoutElement() override { return this; }
        ICanvasRaycastFilterNative* AsRaycastFilter() override { return this; }
        void OnEnable() override;
        void OnDisable() override;
        void OnCanvasHierarchyChanged() override;
        void OnDidApplyAnimationProperties() override;

        engine::Sprite* activeSprite() const { return overrideSprite_.Get() ? overrideSprite_.Get() : sprite_.Get(); }

    private:
        f32 PixelsPerUnitConst() const;
        Vector4 DrawingDimensions(bool preserveAspect);
        void PreserveAspect(Rect& rect, Vector2 spriteSize) const;
        Vector4 AdjustedBorders(Vector4 border, Rect adjusted) const;
        void GenerateSimple(VertexHelper& vh, bool preserveAspect);
        void GenerateSpriteMesh(VertexHelper& vh, bool preserveAspect);
        void GenerateSliced(VertexHelper& vh);
        void GenerateTiled(VertexHelper& vh);
        void GenerateFilled(VertexHelper& vh, bool preserveAspect);
        Vector2 MapCoordinate(Vector2 local, Rect rect);
        bool SupportsAlphaHitTest() const;

        HX_FIELD(SerializeField, Name = "m_Sprite") engine::PPtr<engine::Sprite> sprite_;
        engine::PPtr<engine::Sprite> overrideSprite_;
        HX_FIELD(SerializeField, Name = "m_Type") Type type_ = Type::Simple;
        HX_FIELD(SerializeField, Name = "m_PreserveAspect") bool preserveAspect_ = false;
        HX_FIELD(SerializeField, Name = "m_FillCenter") bool fillCenter_ = true;
        HX_FIELD(SerializeField, Name = "m_FillMethod") FillMethod fillMethod_ = FillMethod::Radial360;
        HX_FIELD(SerializeField, Name = "m_FillAmount") f32 fillAmount_ = 1;
        HX_FIELD(SerializeField, Name = "m_FillClockwise") bool fillClockwise_ = true;
        HX_FIELD(SerializeField, Name = "m_FillOrigin") i32 fillOrigin_ = 0;
        HX_FIELD(SerializeField, Name = "m_UseSpriteMesh") bool useSpriteMesh_ = false;
        HX_FIELD(SerializeField, Name = "m_PixelsPerUnitMultiplier") f32 pixelsPerUnitMultiplier_ = 1;
        f32 alphaHitTestMinimumThreshold_ = 0;
        f32 cachedReferencePixelsPerUnit_ = 100;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI")
    class RawImage : public MaskableGraphic
    {
    public:
        RawImage() = default;
        ~RawImage() override = default;

        HX_PROPERTY() engine::Texture* texture() const { return texture_.Get(); }
        void SetTexture(engine::Texture* value);
        HX_PROPERTY() Rect uvRect() const { return uvRect_; }
        void SetUvRect(Rect value);
        engine::Texture* mainTexture() const override;
        void SetNativeSize() override;
        void OnPopulateMesh(VertexHelper* vh) override;
        void OnDidApplyAnimationProperties() override;

    private:
        HX_FIELD(SerializeField, Name = "m_Texture") engine::PPtr<engine::Texture> texture_;
        HX_FIELD(SerializeField, Name = "m_UVRect") Rect uvRect_ = Rect(0, 0, 1, 1);
    };
}
