#pragma once

#include <Helix/CanvasUI/Core/CanvasUpdateRegistry.h>
#include <Helix/CanvasUI/Rendering/VertexHelper.h>
#include <Helix/Engine/Events/HelixEvent.h>
#include <Helix/Engine/Canvas/Canvas.h>
#include <Helix/Engine/Canvas/CanvasRenderer.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Texture/Texture.h>
#include <Helix/Engine/Transform/RectTransform.h>
#include <Helix/Math/Color.h>

namespace helix::engine
{
    class Camera;
    class Canvas;
    class CanvasRenderer;
    class Material;
    class Mesh;
    class RectTransform;
    class Texture;
    class Texture2D;
}

// Graphic: the base of everything the canvas UI draws. It builds its mesh (OnPopulateMesh, then the IMeshModifier
// components) and its material (the material, then the IMaterialModifier components) into its CanvasRenderer when the
// rebuild loop asks, after something made them dirty. MaskableGraphic is clipped by RectMask2D and masked by Mask.
namespace helix::canvasui
{
    class Graphic;

    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class IMaterialModifier
    {
    public:
        virtual ~IMaterialModifier() = default;
        HX_FUNCTION() virtual engine::Material* GetModifiedMaterial(engine::Material* baseMaterial) = 0;
    };

    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class IMeshModifier
    {
    public:
        virtual ~IMeshModifier() = default;
        HX_FUNCTION() virtual void ModifyMesh(VertexHelper* verts) = 0;
    };

    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class IMaskable
    {
    public:
        virtual ~IMaskable() = default;
        HX_FUNCTION() virtual void RecalculateMasking() = 0;
    };

    // What RectMask2D clips.
    HX_INTERFACE(Namespace = "HelixEngine.CanvasUI")
    class IClippable
    {
    public:
        virtual ~IClippable() = default;
        HX_PROPERTY(Name = "gameObject") virtual engine::GameObject* ClippableGameObject() const = 0;
        HX_PROPERTY(Name = "rectTransform") virtual engine::RectTransform* ClippableRectTransform() const = 0;
        HX_FUNCTION() virtual void RecalculateClipping() = 0;
        HX_FUNCTION() virtual void Cull(Rect clipRect, bool validRect) = 0;
        HX_FUNCTION() virtual void SetClipRect(Rect value, bool validRect) = 0;
        HX_FUNCTION() virtual void SetClipSoftness(Vector2 clipSoftness) = 0;
    };

    // A raycast filter of a component of C++ (C#'s ICanvasRaycastFilter for the canvas UI's own components).
    class ICanvasRaycastFilterNative
    {
    public:
        virtual ~ICanvasRaycastFilterNative() = default;
        virtual bool IsRaycastLocationValid(Vector2 sp, engine::Camera* eventCamera) = 0;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Abstract, RequireComponent = "engine::RectTransform, engine::CanvasRenderer")
    class Graphic : public UIBehaviour, public ICanvasElement
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        Graphic() = default;
        ~Graphic() override;

        // ---------------------------------------------------------------------------------------------- statics
        HX_PROPERTY(ReadOnly) static engine::Material* defaultGraphicMaterial();

        // ---------------------------------------------------------------------------------------------- appearance
        HX_PROPERTY() virtual Color color() const { return color_; }
        virtual void SetColor(Color value);
        HX_PROPERTY() virtual bool raycastTarget() const { return raycastTarget_; }
        virtual void SetRaycastTarget(bool value);
        // Inset (left, bottom, right, top) of the raycast's rectangle.
        HX_PROPERTY() Vector4 raycastPadding() const { return raycastPadding_; }
        void SetRaycastPadding(Vector4 value) { raycastPadding_ = value; }
        HX_PROPERTY(Protected) bool useLegacyMeshGeneration() const { return useLegacyMeshGeneration_; }
        void SetUseLegacyMeshGeneration(bool value) { useLegacyMeshGeneration_ = value; }

        // ---------------------------------------------------------------------------------------------- what it is in
        // Its CanvasRenderer's absolute depth (-1 when not drawn).
        HX_PROPERTY(ReadOnly) i32 depth() const;
        HX_PROPERTY(ReadOnly) engine::RectTransform* rectTransform() const;
        HX_PROPERTY(ReadOnly) engine::Canvas* canvas();
        HX_PROPERTY(ReadOnly) engine::CanvasRenderer* canvasRenderer() const;

        // ---------------------------------------------------------------------------------------------- materials
        HX_PROPERTY(ReadOnly) virtual engine::Material* defaultMaterial() const { return defaultGraphicMaterial(); }
        HX_PROPERTY() virtual engine::Material* material() const;
        virtual void SetMaterial(engine::Material* value);
        // The material with the modifiers of its GameObject applied.
        HX_PROPERTY(ReadOnly) virtual engine::Material* materialForRendering();
        HX_PROPERTY(ReadOnly) virtual engine::Texture* mainTexture() const;

        // ---------------------------------------------------------------------------------------------- dirtiness
        HX_FUNCTION() virtual void SetAllDirty();
        HX_FUNCTION() virtual void SetLayoutDirty();
        HX_FUNCTION() virtual void SetVerticesDirty();
        HX_FUNCTION() virtual void SetMaterialDirty();
        HX_FUNCTION() void SetRaycastDirty();
        HX_FUNCTION(Internal) void RegisterDirtyCallbackInternal(i32 kind, bool add, u64 key);
        // Raised for C#'s callbacks: (graphic, kind 0 layout 1 vertices 2 material).
        HX_EVENT(Internal) static engine::Event<Graphic*, i32>& dirtyCallback();

        // ---------------------------------------------------------------------------------------------- rebuilding
        HX_FUNCTION() void Rebuild(CanvasUpdate update) override;
        HX_FUNCTION() void LayoutComplete() override {}
        HX_FUNCTION() void GraphicUpdateComplete() override {}
        HX_FUNCTION(Protected) virtual void UpdateMaterial();
        HX_FUNCTION(Protected) virtual void UpdateGeometry();
        HX_FUNCTION(Protected) virtual void OnPopulateMesh(VertexHelper* vh);
        HX_FUNCTION() virtual void OnRebuildRequested();
        HX_FUNCTION() virtual void OnCullingChanged();
        HX_FUNCTION() virtual void SetNativeSize() {}

        // ---------------------------------------------------------------------------------------------- raycasts
        // Inside its rectangle (with the padding) as the filters of its GameObject and its parents allow.
        HX_FUNCTION() virtual bool Raycast(Vector2 sp, engine::Camera* eventCamera);
        HX_FUNCTION(Protected, Name = "Raycast") bool RaycastIgnoringMasks(Vector2 sp, engine::Camera* eventCamera, bool ignoreMasks);
        HX_FUNCTION() Vector2 PixelAdjustPoint(Vector2 point);
        HX_FUNCTION() Rect GetPixelAdjustedRect();

        // ---------------------------------------------------------------------------------------------- tweens
        HX_FUNCTION() virtual void CrossFadeColor(Color targetColor, f32 duration, bool ignoreTimeScale, bool useAlpha);
        HX_FUNCTION() virtual void CrossFadeColor(Color targetColor, f32 duration, bool ignoreTimeScale, bool useAlpha, bool useRGB);
        HX_FUNCTION() virtual void CrossFadeAlpha(f32 alpha, f32 duration, bool ignoreTimeScale);
        // The tweens of every graphic one step on (the canvas UI runs them before the rebuilds).
        static void UpdateTweens();

        // ---------------------------------------------------------------------------------------------- the element
        engine::Transform* transform() const override { return engine::Component::transform(); }
        bool IsDestroyed() const override { return UIBehaviour::IsDestroyed(); }
        engine::Object* ElementObject() override { return this; }
        HX_PROPERTY(ReadOnly, Protected, Name = "workerMesh") static engine::Mesh* WorkerMesh();

        // ---------------------------------------------------------------------------------------------- messages
        void OnEnable() override;
        void OnDisable() override;
        void OnDestroy() override;
        void OnRectTransformDimensionsChange() override;
        void OnBeforeTransformParentChanged() override;
        void OnTransformParentChanged() override;
        void OnCanvasHierarchyChanged() override;
        void OnDidApplyAnimationProperties() override { SetAllDirty(); }

        // The canvas it draws in (the nearest enabled one above it), found again.
        void CacheCanvas();
        // The vertex helper of the rebuilds (shared, as the reference's).
        static VertexHelper& SharedVertexHelper();
        bool verticesDirty() const { return vertsDirty_; }
        bool materialDirty() const { return materialDirty_; }

    protected:
        void DoMeshGeneration();
        // The material set on the graphic (null: the default one).
        engine::Material* ownMaterial() const { return material_.Get(); }

        bool skipLayoutUpdate_ = false, skipMaterialUpdate_ = false;
        bool useLegacyMeshGeneration_ = false;

    private:
        HX_FIELD(SerializeField, Name = "m_Material") engine::PPtr<engine::Material> material_;
        HX_FIELD(SerializeField, Name = "m_Color") Color color_ = Color(1, 1, 1, 1);
        HX_FIELD(SerializeField, Name = "m_RaycastTarget") bool raycastTarget_ = true;
        HX_FIELD(SerializeField, Name = "m_RaycastPadding") Vector4 raycastPadding_ = Vector4(0, 0, 0, 0);
        struct Tween
        {
            Color from, to;
            f32 duration = 0, elapsed = 0;
            bool ignoreTimeScale = false, alpha = false, rgb = false, running = false;
        };
        void StartTween(const Tween& t);

        engine::PPtr<engine::Canvas> canvas_;
        bool vertsDirty_ = false, materialDirty_ = false, raycastTargetCache_ = true;
        Tween tween_;
    };

    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Abstract)
    class MaskableGraphic : public Graphic, public IClippable, public IMaskable, public IMaterialModifier
    {
    public:
        // A copy (Instantiate's) starts with its own runtime state.
        void OnAfterCloneNative() override;
        // The event of a change of the cull state (files hold its persistent calls; C# has its own class over it).
        HX_STRUCT(SerializedOnly)
        struct CullStateChangedEvent : engine::HelixEvent<bool>
        {
            static constexpr StringView kTypeName = "CullStateChangedEvent";
        };

        MaskableGraphic();
        ~MaskableGraphic() override;

        HX_PROPERTY() bool maskable() const { return maskable_; }
        void SetMaskable(bool value);
        // A Mask's own graphic (not masked by itself).
        HX_PROPERTY() bool isMaskingGraphic() const { return isMaskingGraphic_; }
        void SetIsMaskingGraphic(bool value);
        CullStateChangedEvent& onCullStateChanged() { return onCullStateChanged_; }
        HX_EVENT(Internal) engine::Event<bool>& onCullStateChangedNative() { return onCullStateChanged_.listeners(); }

        HX_FUNCTION() engine::Material* GetModifiedMaterial(engine::Material* baseMaterial) override;
        HX_FUNCTION() void Cull(Rect clipRect, bool validRect) override;
        HX_FUNCTION() void SetClipRect(Rect clipRect, bool validRect) override;
        HX_FUNCTION() void SetClipSoftness(Vector2 clipSoftness) override;
        HX_FUNCTION() void RecalculateClipping() override;
        HX_FUNCTION() void RecalculateMasking() override;
        HX_FUNCTION() bool Raycast(Vector2 sp, engine::Camera* eventCamera) override;

        engine::GameObject* ClippableGameObject() const override { return gameObject(); }
        engine::RectTransform* ClippableRectTransform() const override { return rectTransform(); }
        IClippable* AsClippable() override { return this; }
        IMaterialModifier* AsMaterialModifier() override { return this; }

        void OnEnable() override;
        void OnDisable() override;
        void OnTransformParentChanged() override;
        void OnCanvasHierarchyChanged() override;

    protected:
        i32 stencilValue_ = 0;
        bool shouldRecalculateStencil_ = true, shouldRecalculate_ = true;
        engine::PPtr<engine::Material> maskMaterial_;

    private:
        void UpdateClipParent();
        void UpdateCull(bool cull);

        HX_FIELD(SerializeField, Name = "m_Maskable") bool maskable_ = true;
        bool isMaskingGraphic_ = false;
        HX_FIELD(SerializeField, Name = "m_OnCullStateChanged") CullStateChangedEvent onCullStateChanged_;
        // The RectMask2D that clips it (registered with it).
        engine::PPtr<engine::Object> parentMask_;
        Rect lastClipRect_;
        bool lastClipValid_ = false;
    };

    // The graphics of each canvas (and those that take raycasts), for the raycasters.
    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static)
    class GraphicRegistry
    {
    public:
        HX_FUNCTION() static void RegisterGraphicForCanvas(engine::Canvas* c, Graphic* graphic);
        HX_FUNCTION() static void RegisterRaycastGraphicForCanvas(engine::Canvas* c, Graphic* graphic);
        HX_FUNCTION() static void UnregisterGraphicForCanvas(engine::Canvas* c, Graphic* graphic);
        HX_FUNCTION() static void UnregisterRaycastGraphicForCanvas(engine::Canvas* c, Graphic* graphic);
        HX_FUNCTION() static void DisableGraphicForCanvas(engine::Canvas* c, Graphic* graphic);
        HX_FUNCTION() static void DisableRaycastGraphicForCanvas(engine::Canvas* c, Graphic* graphic);
        HX_FUNCTION(Internal) static Array<Graphic*> GetGraphicsForCanvasInternal(engine::Canvas* canvas);
        HX_FUNCTION(Internal) static Array<Graphic*> GetRaycastableGraphicsForCanvasInternal(engine::Canvas* canvas);
    };

    // The graphics that rebuild when the fonts' atlases change (the text components).
    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Static)
    class GraphicRebuildTracker
    {
    public:
        HX_FUNCTION() static void TrackGraphic(Graphic* g);
        HX_FUNCTION() static void UnTrackGraphic(Graphic* g);
        HX_FUNCTION() static void DisableTrackGraphic(Graphic* g);
        // Every tracked graphic's vertices dirty (a font's atlas changed).
        static void RebuildAll();
    };
}
