#pragma once

#include <Helix/CanvasUI/Masking/Clipping.h>
#include <Helix/CanvasUI/Rendering/Graphic.h>
#include <Helix/Math/Vector.h>

// Mask: the graphic of its GameObject writes the stencil, the maskable graphics under it draw only there (and its
// graphic is hidden unless showMaskGraphic). RectMask2D: the graphics under it are clipped to its rectangle (less
// the padding, softened at its edges) without the stencil, culled when outside.
namespace helix::canvasui
{
    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::RectTransform")
    class Mask : public UIBehaviour, public IMaterialModifier, public ICanvasRaycastFilterNative
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        Mask() = default;
        ~Mask() override;

        HX_PROPERTY(ReadOnly) engine::RectTransform* rectTransform() const;
        HX_PROPERTY() bool showMaskGraphic() const { return showMaskGraphic_; }
        void SetShowMaskGraphic(bool value);
        HX_PROPERTY(ReadOnly) Graphic* graphic() const;
        HX_FUNCTION() virtual bool MaskEnabled() const { return IsActive() && graphic() != nullptr; }
        HX_FUNCTION(Attributes = "[System.Obsolete(\"Not used anymore.\")]") virtual void OnSiblingGraphicEnabledDisabled() {}
        HX_FUNCTION() bool IsRaycastLocationValid(Vector2 sp, engine::Camera* eventCamera) override;
        HX_FUNCTION() engine::Material* GetModifiedMaterial(engine::Material* baseMaterial) override;

        IMaterialModifier* AsMaterialModifier() override { return this; }
        ICanvasRaycastFilterNative* AsRaycastFilter() override { return this; }
        void OnEnable() override;
        void OnDisable() override;

    private:
        HX_FIELD(SerializeField, Name = "m_ShowMaskGraphic") bool showMaskGraphic_ = true;
        engine::PPtr<engine::Material> maskMaterial_, unmaskMaterial_;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", RequireComponent = "engine::RectTransform")
    class RectMask2D : public UIBehaviour, public IClipper, public ICanvasRaycastFilterNative
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        RectMask2D() = default;
        ~RectMask2D() override;

        // Inset of the clip (left, bottom, right, top).
        HX_PROPERTY() Vector4 padding() const { return padding_; }
        void SetPadding(Vector4 value);
        // The fade at the clip's edges, in canvas units.
        HX_PROPERTY() Vector2Int softness() const { return softness_; }
        void SetSoftness(Vector2Int value);
        // Its rectangle in its canvas's space.
        HX_PROPERTY(ReadOnly) Rect canvasRect();
        HX_PROPERTY(ReadOnly) engine::RectTransform* rectTransform() const;
        HX_FUNCTION() bool IsRaycastLocationValid(Vector2 sp, engine::Camera* eventCamera) override;
        HX_FUNCTION() void PerformClipping() override;
        HX_FUNCTION() virtual void UpdateClipSoftness();
        HX_FUNCTION() void AddClippable(IClippable* clippable);
        HX_FUNCTION() void RemoveClippable(IClippable* clippable);

        engine::Object* ClipperObject() override { return this; }
        ICanvasRaycastFilterNative* AsRaycastFilter() override { return this; }
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;
        void OnTransformParentChanged() override;
        void OnCanvasHierarchyChanged() override;

        // Its rectangle less the padding in the space of the canvas the clip is in (the root canvas).
        Rect PaddedCanvasRect();

    private:
        engine::Canvas* RootCanvas();

        HX_FIELD(SerializeField, Name = "m_Padding") Vector4 padding_ = Vector4(0, 0, 0, 0);
        HX_FIELD(SerializeField, Name = "m_Softness") Vector2Int softness_ = Vector2Int(0, 0);
        struct Clippable
        {
            IClippable* clippable = nullptr;
            engine::PPtr<engine::Object> object;
        };
        Array<Clippable> clippables_;
        Array<RectMask2D*> clippers_;
        Rect lastClipRect_;
        bool lastValid_ = false, forceClip_ = true, shouldRecalculateClipRects_ = true;
        engine::PPtr<engine::Canvas> canvas_;
    };
}
