#include <Support/CanvasTestKit.h>

#include <Helix/CanvasUI/Layout/LayoutGroups.h>
#include <Helix/CanvasUI/Rendering/Image.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Testing/Test.h>

#include <cstdio>

using namespace helix;
using namespace helix::canvasui;
using namespace helix::canvasui::testing;
using engine::GameObject;

namespace
{
    // An element of fixed sizes (a LayoutElement) under the group.
    GameObject* Sized(StringView name, engine::Transform* parent, f32 minW, f32 prefW, f32 flexW, f32 prefH)
    {
        GameObject* g = Element(name, parent, Vector2(0, 0), Vector2(10, 10));
        LayoutElement* e = Add<LayoutElement>(g);
        e->SetMinWidth(minW);
        e->SetPreferredWidth(prefW);
        e->SetFlexibleWidth(flexW);
        e->SetPreferredHeight(prefH);
        return g;
    }

    // The element's rectangle in its parent's space from the parent's top left (x right, y down).
    Rect Placed(GameObject* g)
    {
        engine::RectTransform* r = Rt(g);
        engine::RectTransform* parent = static_cast<engine::RectTransform*>(r->parent());
        Rect p = parent->rect();
        Vector3 local = r->localPosition();
        Rect own = r->rect();
        return Rect(local.x + own.x - p.x, (p.y + p.height) - (local.y + own.y + own.height), own.width, own.height);
    }

    bool NearRect(Rect a, Rect b)
    {
        bool ok = Near(a.x, b.x) && Near(a.y, b.y) && Near(a.width, b.width) && Near(a.height, b.height);
        if (!ok)
            std::printf("    rect %g %g %g %g, expected %g %g %g %g\n", a.x, a.y, a.width, a.height, b.x, b.y, b.width, b.height);
        return ok;
    }
}

HX_TEST(Layout, HorizontalAndVerticalGroupsPlaceTheirChildren)
{
    HX_REQUIRE(RegisterModules());
    engine::Camera::SetDisplaySize(400, 400);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    GameObject* row = Element("Row", canvas->transform(), Vector2(0, 0), Vector2(200, 50));
    HorizontalLayoutGroup* h = Add<HorizontalLayoutGroup>(row);
    h->padding()->SetLeft(10), h->padding()->SetRight(10), h->padding()->SetTop(5), h->padding()->SetBottom(5);
    h->SetSpacing(4);
    h->SetChildForceExpandWidth(false);
    h->SetChildForceExpandHeight(false);
    GameObject* a = Sized("A", row->transform(), 20, 40, 0, 30);
    GameObject* b = Sized("B", row->transform(), 20, 30, 1, 20);
    GameObject* c = Sized("C", row->transform(), 10, 10, 0, 40);
    Frame();
    // The preferred size (40 + 30 + 10, spacing 8, padding 20) leaves 92 of the 200: B, the only flexible one, takes it.
    HX_CHECK(NearRect(Placed(a), Rect(10, 5, 40, 30)) && NearRect(Placed(b), Rect(54, 5, 122, 20)) && NearRect(Placed(c), Rect(180, 5, 10, 40)));
    HX_CHECK(Near(h->minWidth(), 20 + 20 + 10 + 8 + 20) && Near(h->preferredWidth(), 40 + 30 + 10 + 8 + 20) && Near(h->preferredHeight(), 50));
    // Driven: the children's anchors and sizes belong to the group.
    HX_CHECK(Rt(a)->drivenByObject() == h);
    // Reversed and centered (the middle row of the alignment), heights forced to the row.
    h->SetReverseArrangement(true);
    h->SetChildAlignment(ui::gui::TextAnchor::MiddleCenter);
    h->SetChildForceExpandHeight(true);
    Frame();
    HX_CHECK(Near(Placed(c).x, 10) && Near(Placed(a).height, 40) && Near(Placed(a).y, 5));
    // Children of fixed sizes (no control): their own sizes kept, aligned in their cells.
    h->SetChildControlWidth(false);
    h->SetChildControlHeight(false);
    Rt(a)->SetSizeDelta(Vector2(12, 12));
    Frame();
    HX_CHECK(Near(Placed(a).width, 12) && Near(Placed(a).height, 12));
    // An ignored child keeps out of it.
    h->SetChildControlWidth(true);
    h->SetChildControlHeight(true);
    h->SetReverseArrangement(false);
    h->SetChildAlignment(ui::gui::TextAnchor::UpperLeft);
    static_cast<LayoutElement*>(a->GetComponent(scripting::TypeOf<LayoutElement>()))->SetIgnoreLayout(true);
    Frame();
    HX_CHECK(Near(Placed(b).x, 10));
    // A vertical group with a content size fitter: its height its children's preferred heights.
    GameObject* column = Element("Column", canvas->transform(), Vector2(0, 0), Vector2(100, 10));
    VerticalLayoutGroup* v = Add<VerticalLayoutGroup>(column);
    v->SetSpacing(2);
    v->SetChildForceExpandHeight(false);
    ContentSizeFitter* fit = Add<ContentSizeFitter>(column);
    fit->SetVerticalFit(ContentSizeFitter::FitMode::PreferredSize);
    GameObject* x = Sized("X", column->transform(), 0, 0, 0, 25);
    GameObject* y = Sized("Y", column->transform(), 0, 0, 0, 15);
    Frame();
    HX_CHECK(Near(Rt(column)->rect().height, 42) && NearRect(Placed(x), Rect(0, 0, 100, 25)) && NearRect(Placed(y), Rect(0, 27, 100, 15)));
    HX_CHECK(Near(LayoutUtility::GetPreferredHeight(Rt(column)), 42) && Near(LayoutUtility::GetFlexibleWidth(Rt(x)), 0));
    // An immediate rebuild without the loop.
    static_cast<LayoutElement*>(y->GetComponent(scripting::TypeOf<LayoutElement>()))->SetPreferredHeight(35);
    LayoutRebuilder::ForceRebuildLayoutImmediate(Rt(column));
    HX_CHECK(Near(Rt(column)->rect().height, 62));
    DestroyImmediate(canvas->gameObject());
    engine::Camera::SetDisplaySize(1, 1);
    engine::testing::Cleanup();
}

HX_TEST(Layout, GridsFittersAndTheScaler)
{
    HX_REQUIRE(RegisterModules());
    engine::Camera::SetDisplaySize(800, 400);
    engine::Canvas* canvas = MakeCanvas("Canvas");
    GameObject* grid = Element("Grid", canvas->transform(), Vector2(0, 0), Vector2(250, 200));
    GridLayoutGroup* g = Add<GridLayoutGroup>(grid);
    g->SetCellSize(Vector2(50, 40));
    g->SetSpacing(Vector2(10, 5));
    GameObject* cells[5];
    for (i32 i = 0; i < 5; i++)
        cells[i] = Element("Cell", grid->transform(), Vector2(0, 0), Vector2(1, 1));
    Frame();
    // Four columns fit 250 (4 * 50 + 3 * 10): row by row from the top left.
    HX_CHECK(NearRect(Placed(cells[0]), Rect(0, 0, 50, 40)) && NearRect(Placed(cells[3]), Rect(180, 0, 50, 40)) && NearRect(Placed(cells[4]), Rect(0, 45, 50, 40)));
    // Two fixed columns from the lower right, columns first.
    g->SetConstraint(GridLayoutGroup::Constraint::FixedColumnCount);
    g->SetConstraintCount(2);
    g->SetStartCorner(GridLayoutGroup::Corner::LowerRight);
    g->SetStartAxis(GridLayoutGroup::Axis::Vertical);
    Frame();
    // 5 cells in 2 columns: 3 rows; columns first from the bottom right: the fourth cell starts the left column.
    HX_CHECK(NearRect(Placed(cells[0]), Rect(60, 90, 50, 40)) && NearRect(Placed(cells[2]), Rect(60, 0, 50, 40)) && NearRect(Placed(cells[3]), Rect(0, 90, 50, 40)));
    HX_CHECK(Near(g->preferredWidth(), 110) && Near(g->minHeight(), 130));
    // Aspect ratio: fit a 2:1 box in its 250 x 200 parent.
    GameObject* box = Element("Box", grid->transform(), Vector2(0, 0), Vector2(10, 10));
    LayoutElement* ignored = Add<LayoutElement>(box);
    ignored->SetIgnoreLayout(true);
    AspectRatioFitter* aspect = Add<AspectRatioFitter>(box);
    aspect->SetAspectRatio(2);
    aspect->SetAspectMode(AspectRatioFitter::AspectMode::FitInParent);
    HX_CHECK(Near(Rt(box)->rect().width, 250) && Near(Rt(box)->rect().height, 125));
    aspect->SetAspectMode(AspectRatioFitter::AspectMode::EnvelopeParent);
    HX_CHECK(Near(Rt(box)->rect().width, 400) && Near(Rt(box)->rect().height, 200));
    aspect->SetAspectMode(AspectRatioFitter::AspectMode::WidthControlsHeight);
    HX_CHECK(Near(Rt(box)->rect().height, Rt(box)->rect().width / 2));
    HX_CHECK(aspect->IsComponentValidOnObject() && aspect->IsAspectModeValid());
    // The scaler: 800 x 400 against 1600 x 800 is half; matching the width (0) or the height (1) agree; Expand and
    // Shrink against 800 x 800: 0.5 and 1.
    CanvasScaler* scaler = Add<CanvasScaler>(canvas->gameObject());
    scaler->SetUiScaleMode(CanvasScaler::ScaleMode::ScaleWithScreenSize);
    scaler->SetReferenceResolution(Vector2(1600, 800));
    Frame();
    HX_CHECK(Near(canvas->scaleFactor(), 0.5f) && Near(Rt(canvas->gameObject())->rect().width, 1600));
    scaler->SetReferenceResolution(Vector2(800, 800));
    scaler->SetScreenMatchMode(CanvasScaler::ScreenMatchMode::Expand);
    Frame();
    HX_CHECK(Near(canvas->scaleFactor(), 0.5f));
    scaler->SetScreenMatchMode(CanvasScaler::ScreenMatchMode::Shrink);
    Frame();
    HX_CHECK(Near(canvas->scaleFactor(), 1));
    // Match halfway in log space: sqrt(1 * 0.5).
    scaler->SetScreenMatchMode(CanvasScaler::ScreenMatchMode::MatchWidthOrHeight);
    scaler->SetMatchWidthOrHeight(0.5f);
    Frame();
    HX_CHECK(Near(canvas->scaleFactor(), std::sqrt(0.5f)));
    // Constant physical size at the fallback 96 dpi in points (72 per inch): 96 / 72.
    scaler->SetUiScaleMode(CanvasScaler::ScaleMode::ConstantPhysicalSize);
    Frame();
    HX_CHECK(Near(canvas->scaleFactor(), engine::Screen::dpi() == 0 ? 96.0f / 72.0f : engine::Screen::dpi() / 72.0f));
    scaler->SetUiScaleMode(CanvasScaler::ScaleMode::ConstantPixelSize);
    scaler->SetScaleFactorProperty(2);
    Frame();
    HX_CHECK(Near(canvas->scaleFactor(), 2));
    // Off: back to 1.
    scaler->SetEnabled(false);
    HX_CHECK(Near(canvas->scaleFactor(), 1));
    DestroyImmediate(canvas->gameObject());
    engine::Camera::SetDisplaySize(1, 1);
    engine::testing::Cleanup();
}
