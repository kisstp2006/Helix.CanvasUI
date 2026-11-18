#include <Support/CanvasTestKit.h>

#include <Helix/CanvasUI/Effects/MeshEffects.h>
#include <Helix/CanvasUI/Masking/Masks.h>
#include <Helix/CanvasUI/Rendering/Image.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::canvasui;
using namespace helix::canvasui::testing;
using engine::GameObject;

namespace
{
    Span<const engine::UIVertex> VerticesOf(Graphic* g) { return g->canvasRenderer()->vertices(); }

    // The axis aligned bounds of a graphic's vertices.
    Rect BoundsOf(Graphic* g)
    {
        Span<const engine::UIVertex> v = VerticesOf(g);
        if (v.Empty())
            return Rect(0, 0, 0, 0);
        f32 x0 = v[0].position.x, y0 = v[0].position.y, x1 = x0, y1 = y0;
        for (const engine::UIVertex& p : v)
        {
            x0 = p.position.x < x0 ? p.position.x : x0, x1 = p.position.x > x1 ? p.position.x : x1;
            y0 = p.position.y < y0 ? p.position.y : y0, y1 = p.position.y > y1 ? p.position.y : y1;
        }
        return Rect(x0, y0, x1 - x0, y1 - y0);
    }
}

HX_TEST(Graphic, TheRebuildLoopBuildsMeshesMaterialsAndRegistries)
{
    HX_REQUIRE(RegisterModules());
    engine::Camera::SetDisplaySize(200, 100);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    GameObject* g = Element("Image", canvas->transform(), Vector2(10, 0), Vector2(40, 20));
    Image* image = Add<Image>(g);
    HX_REQUIRE(image && image->canvasRenderer() && image->canvas() == canvas);
    // Dirty on enable: the loop builds its quad (the rectangle around the pivot) and its material.
    Frame();
    Span<const engine::UIVertex> v = VerticesOf(image);
    HX_CHECK(v.Size() == 4 && Near(v[0].position, Vector3(-20, -10, 0)) && Near(v[2].position, Vector3(20, 10, 0)));
    HX_CHECK(image->canvasRenderer()->GetMaterial() == engine::Canvas::GetDefaultCanvasMaterial() && image->depth() == 0);
    HX_CHECK(image->mainTexture() == engine::Texture2D::whiteTexture() && !image->verticesDirty() && !image->materialDirty());
    // A color change: the vertices again.
    image->SetColor(Color(1, 0, 0, 0.5f));
    HX_CHECK(image->verticesDirty());
    Frame();
    HX_CHECK(VerticesOf(image)[1].color == Color32(255, 0, 0, 128));
    // A size change rebuilds through OnRectTransformDimensionsChange.
    Rt(g)->SetSizeDelta(Vector2(60, 20));
    Frame();
    HX_CHECK(Near(VerticesOf(image)[2].position, Vector3(30, 10, 0)));
    // Registries: drawn and raycast by its canvas.
    HX_CHECK(GraphicRegistry::GetGraphicsForCanvasInternal(canvas).Size() == 1 && GraphicRegistry::GetRaycastableGraphicsForCanvasInternal(canvas).Size() == 1);
    image->SetRaycastTarget(false);
    HX_CHECK(GraphicRegistry::GetRaycastableGraphicsForCanvasInternal(canvas).Empty());
    image->SetRaycastTarget(true);
    // Raycast filters: a group that blocks no raycasts lets them through, one that does not stops them.
    HX_CHECK(image->Raycast(Vector2(0, 0), nullptr));
    engine::CanvasGroup* group = canvas->gameObject()->AddComponent<engine::CanvasGroup>();
    group->SetBlocksRaycasts(false);
    HX_CHECK(!image->Raycast(Vector2(0, 0), nullptr));
    // A group below that ignores its parents decides alone.
    engine::CanvasGroup* own = g->AddComponent<engine::CanvasGroup>();
    own->SetIgnoreParentGroups(true);
    HX_CHECK(image->Raycast(Vector2(0, 0), nullptr));
    // Disabled: its renderer cleared, out of the registries.
    image->SetEnabled(false);
    HX_CHECK(image->canvasRenderer()->vertices().Empty() && GraphicRegistry::GetGraphicsForCanvasInternal(canvas).Empty());
    image->SetEnabled(true);
    Frame();
    HX_CHECK(VerticesOf(image).Size() == 4);
    // A cross fade of no time: there at once, the alpha only.
    image->CrossFadeAlpha(0, 0, true);
    HX_CHECK(image->canvasRenderer()->GetColor().a == 0 && image->canvasRenderer()->GetColor().r == 1);
    DestroyImmediate(canvas->gameObject());
    engine::Camera::SetDisplaySize(1, 1);
    engine::testing::Cleanup();
}

HX_TEST(Image, EveryTypeBuildsItsMesh)
{
    HX_REQUIRE(RegisterModules());
    engine::Camera::SetDisplaySize(200, 200);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    engine::Texture2D* texture = FramedTexture(32, 8, Color32(255, 0, 0, 255), Color32(0, 0, 255, 255));
    engine::Sprite* sprite = SpriteOf(texture, Vector4(8, 8, 8, 8));
    GameObject* g = Element("Image", canvas->transform(), Vector2(0, 0), Vector2(100, 60));
    Image* image = Add<Image>(g);
    image->SetSprite(sprite);
    Frame();
    // Simple: one quad over the rectangle with the sprite's uvs.
    HX_CHECK(VerticesOf(image).Size() == 4 && Near(VerticesOf(image)[2].uv0.x, 1) && image->mainTexture() == texture);
    // Preserve aspect: a square in the middle (the pivot's place).
    image->SetPreserveAspect(true);
    Frame();
    HX_CHECK(Near(BoundsOf(image).width, 60) && Near(BoundsOf(image).x, -30));
    image->SetPreserveAspect(false);
    // Sliced: nine quads, corners of the borders' size (8 pixels at 100 pixels per unit of 100 reference: 8 units).
    image->SetType(Image::Type::Sliced);
    Frame();
    HX_CHECK(VerticesOf(image).Size() == 36 && Near(VerticesOf(image)[2].position.x, -42) && image->hasBorder());
    image->SetFillCenter(false);
    Frame();
    HX_CHECK(VerticesOf(image).Size() == 32);
    image->SetFillCenter(true);
    // Sliced smaller than its borders: the borders shrink to fit.
    Rt(g)->SetSizeDelta(Vector2(10, 60));
    Frame();
    HX_CHECK(Near(BoundsOf(image).width, 10));
    Rt(g)->SetSizeDelta(Vector2(100, 60));
    // Tiled (a clamped texture with borders): tiles of 16 units inside the borders, the edges and the corners.
    image->SetType(Image::Type::Tiled);
    Frame();
    // Inside 84 x 44: 6 x 3 tiles, sides 3 + 3 + 6 + 6 tiles, 4 corners.
    HX_CHECK(VerticesOf(image).Size() == (18 + 18 + 4) * 4);
    // Filled horizontally from the right by a quarter.
    image->SetType(Image::Type::Filled);
    image->SetFillMethod(Image::FillMethod::Horizontal);
    image->SetFillOrigin(1);
    image->SetFillAmount(0.25f);
    Frame();
    HX_CHECK(Near(BoundsOf(image).x, 25) && Near(BoundsOf(image).width, 25) && Near(VerticesOf(image)[0].uv0.x, 0.75f));
    // Vertically from the bottom by a half.
    image->SetFillMethod(Image::FillMethod::Vertical);
    image->SetFillAmount(0.5f);
    Frame();
    HX_CHECK(Near(BoundsOf(image).height, 30) && Near(BoundsOf(image).y, -30) && image->fillOrigin() == 0);
    // Radial 360 by a quarter from the bottom, clockwise: the quarter left of the bottom's middle (one quad cut).
    image->SetFillMethod(Image::FillMethod::Radial360);
    image->SetFillAmount(0.25f);
    Frame();
    HX_CHECK(VerticesOf(image).Size() == 4 && Near(BoundsOf(image).xMax(), 0) && Near(BoundsOf(image).yMax(), 0));
    // A little more: a second quarter starts.
    image->SetFillAmount(0.3f);
    Frame();
    HX_CHECK(VerticesOf(image).Size() == 8);
    // Radial 90 by a half from the bottom left: a triangle of the rectangle (a quad with two equal corners).
    image->SetFillMethod(Image::FillMethod::Radial90);
    image->SetFillAmount(0.5f);
    Frame();
    HX_CHECK(VerticesOf(image).Size() == 4);
    // Radial 180: two halves.
    image->SetFillMethod(Image::FillMethod::Radial180);
    image->SetFillAmount(0.75f);
    Frame();
    HX_CHECK(VerticesOf(image).Size() == 8);
    image->SetFillAmount(0);
    Frame();
    HX_CHECK(VerticesOf(image).Empty());
    // The sprite's own mesh (a full rectangle sprite: 4 vertices).
    image->SetType(Image::Type::Simple);
    image->SetUseSpriteMesh(true);
    Frame();
    HX_CHECK(VerticesOf(image).Size() == sprite->vertices().Size());
    // Layout element: the sprite's size (sliced: its borders).
    HX_CHECK(Near(image->preferredWidth(), 32) && Near(image->flexibleWidth(), -1));
    image->SetType(Image::Type::Sliced);
    HX_CHECK(Near(image->preferredWidth(), 16));
    // Native size: the sprite's pixels.
    image->SetNativeSize();
    HX_CHECK(Near(Rt(g)->sizeDelta(), Vector2(32, 32)));
    // Alpha hit test on a readable sprite.
    image->SetType(Image::Type::Simple);
    image->SetAlphaHitTestMinimumThreshold(0.5f);
    HX_CHECK(image->IsRaycastLocationValid(Vector2(100, 100), nullptr));
    DestroyImmediate(canvas->gameObject());
    DestroyImmediate(sprite);
    DestroyImmediate(texture);
    engine::Camera::SetDisplaySize(1, 1);
    engine::testing::Cleanup();
}

HX_TEST(Masks, RectMask2DClipsAndCullsMaskUsesTheStencil)
{
    HX_REQUIRE(RegisterModules());
    engine::Camera::SetDisplaySize(200, 200);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    GameObject* clip = Element("Clip", canvas->transform(), Vector2(0, 0), Vector2(100, 100));
    RectMask2D* rectMask = Add<RectMask2D>(clip);
    GameObject* inside = Element("Inside", clip->transform(), Vector2(40, 0), Vector2(40, 40));
    Image* a = Add<Image>(inside);
    GameObject* outside = Element("Outside", clip->transform(), Vector2(200, 0), Vector2(20, 20));
    Image* b = Add<Image>(outside);
    Frame();
    // The clip rectangle in the canvas's space; the image outside is culled.
    engine::CanvasRenderer* ra = a->canvasRenderer();
    HX_CHECK(ra->hasRectClipping() && Near(ra->clipRect().x, -50) && Near(ra->clipRect().width, 100) && !ra->cull());
    HX_CHECK(b->canvasRenderer()->cull());
    // Padding and softness.
    rectMask->SetPadding(Vector4(10, 0, 10, 0));
    rectMask->SetSoftness(Vector2Int(4, 2));
    Frame();
    HX_CHECK(Near(ra->clipRect().x, -40) && Near(ra->clipRect().width, 80) && Near(ra->clippingSoftness(), Vector2(4, 2)));
    // A raycast through the mask: inside its padded rectangle only (a point of the screen: the canvas centered).
    HX_CHECK(rectMask->IsRaycastLocationValid(Vector2(100, 100), nullptr) && !rectMask->IsRaycastLocationValid(Vector2(145, 100), nullptr));
    // Disabled: no clip.
    rectMask->SetEnabled(false);
    Frame();
    HX_CHECK(!ra->hasRectClipping() && !b->canvasRenderer()->cull());
    rectMask->SetEnabled(true);
    // A stencil mask: its graphic writes the stencil (a pop material undoes it), the child reads it.
    GameObject* masker = Element("Mask", canvas->transform(), Vector2(0, 0), Vector2(50, 50));
    Image* maskImage = Add<Image>(masker);
    Mask* mask = Add<Mask>(masker);
    GameObject* child = Element("Masked", masker->transform(), Vector2(0, 0), Vector2(80, 80));
    Image* masked = Add<Image>(child);
    Frame();
    engine::Material* own = maskImage->canvasRenderer()->GetMaterial();
    engine::Material* read = masked->canvasRenderer()->GetMaterial();
    HX_CHECK(own != engine::Canvas::GetDefaultCanvasMaterial() && own->GetFloat("_StencilComp") == 8.0f && own->GetFloat("_StencilOp") == 2.0f);
    HX_CHECK(read->GetFloat("_StencilComp") == 3.0f && read->GetFloat("_Stencil") == 1.0f && read->GetFloat("_StencilWriteMask") == 0.0f);
    HX_CHECK(maskImage->canvasRenderer()->hasPopInstruction() && maskImage->canvasRenderer()->GetPopMaterial(0)->GetFloat("_StencilOp") == 1.0f);
    HX_CHECK(mask->MaskEnabled() && maskImage->isMaskingGraphic() && StencilMaterial::Count() == 3);
    // The mask graphic hidden: its color mask 0.
    mask->SetShowMaskGraphic(false);
    Frame();
    HX_CHECK(maskImage->canvasRenderer()->GetMaterial()->GetFloat("_ColorMask") == 0.0f);
    // Not maskable: the plain material.
    masked->SetMaskable(false);
    Frame();
    HX_CHECK(masked->canvasRenderer()->GetMaterial() == engine::Canvas::GetDefaultCanvasMaterial());
    // Disabled: no stencil left.
    mask->SetEnabled(false);
    masked->SetMaskable(true);
    Frame();
    HX_CHECK(maskImage->canvasRenderer()->GetMaterial() == engine::Canvas::GetDefaultCanvasMaterial() && !maskImage->canvasRenderer()->hasPopInstruction());
    HX_CHECK(masked->canvasRenderer()->GetMaterial() == engine::Canvas::GetDefaultCanvasMaterial() && StencilMaterial::Count() == 0);
    DestroyImmediate(canvas->gameObject());
    engine::Camera::SetDisplaySize(1, 1);
    engine::testing::Cleanup();
}

HX_TEST(MeshEffects, ShadowOutlineAndPositionAsUV1)
{
    HX_REQUIRE(RegisterModules());
    engine::Camera::SetDisplaySize(200, 200);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    GameObject* g = Element("Image", canvas->transform(), Vector2(0, 0), Vector2(20, 20));
    Image* image = Add<Image>(g);
    Shadow* shadow = Add<Shadow>(g);
    Frame();
    // The shadow first (moved, in its color with the graphic's alpha), the graphic after it: 4 + 4 shared vertices.
    Span<const engine::UIVertex> v = VerticesOf(image);
    HX_CHECK(v.Size() == 8 && v[0].color == Color32(0, 0, 0, 128) && Near(v[0].position, Vector3(-9, -11, 0)) && v[4].color == Color32(255, 255, 255, 255));
    shadow->SetEffectDistance(Vector2(1000, 3));
    HX_CHECK(Near(shadow->effectDistance(), Vector2(600, 3)));
    shadow->SetEnabled(false);
    Frame();
    HX_CHECK(VerticesOf(image).Size() == 4);
    DestroyImmediate(shadow);
    Outline* outline = Add<Outline>(g);
    Frame();
    HX_CHECK(VerticesOf(image).Size() == 20);
    DestroyImmediate(outline);
    Add<PositionAsUV1>(g);
    Frame();
    HX_CHECK(Near(VerticesOf(image)[2].uv1.x, 10) && Near(VerticesOf(image)[2].uv1.y, 10));
    DestroyImmediate(canvas->gameObject());
    engine::Camera::SetDisplaySize(1, 1);
    engine::testing::Cleanup();
}
