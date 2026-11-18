#include <Helix/CanvasUI/Rendering/Image.h>

#include <Helix/CanvasUI/Core/Implementation/CanvasUILog.h>
#include <Helix/Engine/Transform/RectTransformUtility.h>

#include <cmath>

namespace helix::canvasui
{
    namespace
    {
        f32 Lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
        f32 Clamp01(f32 v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
        f32 Get(Vector4 v, i32 i) { return i == 0 ? v.x : i == 1 ? v.y : i == 2 ? v.z : v.w; }
        void Put(Vector4& v, i32 i, f32 x) { (i == 0 ? v.x : i == 1 ? v.y : i == 2 ? v.z : v.w) = x; }
        f32 Get(Vector2 v, i32 i) { return i == 0 ? v.x : v.y; }
        void Put(Vector2& v, i32 i, f32 x) { (i == 0 ? v.x : v.y) = x; }

        void Quad(VertexHelper& vh, Vector2 posMin, Vector2 posMax, Color32 color, Vector2 uvMin, Vector2 uvMax)
        {
            i32 start = vh.currentVertCount();
            vh.AddVert(Vector3(posMin.x, posMin.y, 0), color, Vector4(uvMin.x, uvMin.y, 0, 0));
            vh.AddVert(Vector3(posMin.x, posMax.y, 0), color, Vector4(uvMin.x, uvMax.y, 0, 0));
            vh.AddVert(Vector3(posMax.x, posMax.y, 0), color, Vector4(uvMax.x, uvMax.y, 0, 0));
            vh.AddVert(Vector3(posMax.x, posMin.y, 0), color, Vector4(uvMax.x, uvMin.y, 0, 0));
            vh.AddTriangle(start, start + 1, start + 2);
            vh.AddTriangle(start + 2, start + 3, start);
        }

        void Quad(VertexHelper& vh, const Vector2 (&xy)[4], Color32 color, const Vector2 (&uv)[4])
        {
            i32 start = vh.currentVertCount();
            for (int i = 0; i < 4; i++)
                vh.AddVert(Vector3(xy[i].x, xy[i].y, 0), color, Vector4(uv[i].x, uv[i].y, 0, 0));
            vh.AddTriangle(start, start + 1, start + 2);
            vh.AddTriangle(start + 2, start + 3, start);
        }

        // One corner's quad cut by the share of its quarter turn: the corners of the quad (clockwise from i0) moved so
        // the edge from the cut corner runs at the angle (cos, sin), the far corners along the quad's edges.
        void Cut(Vector2 (&p)[4], f32 cos, f32 sin, bool invert, i32 corner)
        {
            i32 i0 = corner, i1 = (corner + 1) % 4, i2 = (corner + 2) % 4, i3 = (corner + 3) % 4;
            if ((corner & 1) == 1)
            {
                if (sin > cos)
                {
                    cos /= sin, sin = 1;
                    if (invert)
                        p[i1].x = Lerp(p[i0].x, p[i2].x, cos), p[i2].x = p[i1].x;
                }
                else if (cos > sin)
                {
                    sin /= cos, cos = 1;
                    if (!invert)
                        p[i2].y = Lerp(p[i0].y, p[i2].y, sin), p[i3].y = p[i2].y;
                }
                else
                    cos = 1, sin = 1;
                if (!invert)
                    p[i3].x = Lerp(p[i0].x, p[i2].x, cos);
                else
                    p[i1].y = Lerp(p[i0].y, p[i2].y, sin);
            }
            else
            {
                if (cos > sin)
                {
                    sin /= cos, cos = 1;
                    if (!invert)
                        p[i1].y = Lerp(p[i0].y, p[i2].y, sin), p[i2].y = p[i1].y;
                }
                else if (sin > cos)
                {
                    cos /= sin, sin = 1;
                    if (invert)
                        p[i2].x = Lerp(p[i0].x, p[i2].x, cos), p[i3].x = p[i2].x;
                }
                else
                    cos = 1, sin = 1;
                if (invert)
                    p[i3].y = Lerp(p[i0].y, p[i2].y, sin);
                else
                    p[i1].x = Lerp(p[i0].x, p[i2].x, cos);
            }
        }

        // The quad (positions and uvs) cut to the fill of its quarter turn; false when nothing of it shows.
        bool RadialCut(Vector2 (&xy)[4], Vector2 (&uv)[4], f32 fill, bool invert, i32 corner)
        {
            if (fill < 0.001f)
                return false;
            if ((corner & 1) == 1)
                invert = !invert;
            if (!invert && fill > 0.999f)
                return true;
            f32 angle = Clamp01(fill);
            if (invert)
                angle = 1 - angle;
            angle *= 90.0f * 3.14159265358979f / 180.0f;
            f32 c = std::cos(angle), s = std::sin(angle);
            Cut(xy, c, s, invert, corner);
            Cut(uv, c, s, invert, corner);
            return true;
        }
    }

    engine::Material* Image::defaultETC1GraphicMaterial() { return engine::Canvas::GetETC1SupportedCanvasMaterial(); }

    // ----------------------------------------------------------------------------------------------- properties

    void Image::SetSprite(engine::Sprite* value)
    {
        engine::Sprite* current = sprite_.Get();
        if (current == value)
            return;
        Vector2 now = current ? Vector2(current->rect().width, current->rect().height) : Vector2(0, 0);
        Vector2 next = value ? Vector2(value->rect().width, value->rect().height) : Vector2(0, 0);
        if (current)
        {
            skipLayoutUpdate_ = now.x == next.x && now.y == next.y;
            skipMaterialUpdate_ = current->texture() == (value ? value->texture() : nullptr);
        }
        else
        {
            skipLayoutUpdate_ = next.x == 0 && next.y == 0;
            skipMaterialUpdate_ = value->texture() == nullptr;
        }
        sprite_ = engine::PPtr<engine::Sprite>(value);
        if (!SupportsAlphaHitTest() && alphaHitTestMinimumThreshold_ > 0)
        {
            alphaHitTestMinimumThreshold_ = 0;
            HX_LOG_WARNING(CanvasUILog, "Sprite was changed for one not readable or with Crunch Compression. Resetting the AlphaHitThreshold to 0.");
        }
        SetAllDirty();
    }

    void Image::SetOverrideSprite(engine::Sprite* value)
    {
        if (overrideSprite_.Get() == value)
            return;
        overrideSprite_ = engine::PPtr<engine::Sprite>(value);
        SetAllDirty();
    }

    void Image::SetType(Type value)
    {
        if (type_ != value)
            type_ = value, SetVerticesDirty();
    }

    void Image::SetPreserveAspect(bool value)
    {
        if (preserveAspect_ != value)
            preserveAspect_ = value, SetVerticesDirty();
    }

    void Image::SetFillCenter(bool value)
    {
        if (fillCenter_ != value)
            fillCenter_ = value, SetVerticesDirty();
    }

    void Image::SetFillMethod(FillMethod value)
    {
        if (fillMethod_ != value)
        {
            fillMethod_ = value;
            SetVerticesDirty();
            fillOrigin_ = 0;
        }
    }

    void Image::SetFillAmount(f32 value)
    {
        value = Clamp01(value);
        if (fillAmount_ != value)
            fillAmount_ = value, SetVerticesDirty();
    }

    void Image::SetFillClockwise(bool value)
    {
        if (fillClockwise_ != value)
            fillClockwise_ = value, SetVerticesDirty();
    }

    void Image::SetFillOrigin(i32 value)
    {
        if (fillOrigin_ != value)
            fillOrigin_ = value, SetVerticesDirty();
    }

    bool Image::SupportsAlphaHitTest() const
    {
        engine::Sprite* s = sprite_.Get();
        return s && s->texture() && s->texture()->isReadable();
    }

    void Image::SetAlphaHitTestMinimumThreshold(f32 value)
    {
        if (sprite_.Get() && !SupportsAlphaHitTest())
        {
            HX_LOG_ERROR(CanvasUILog, "The sprite does not support alpha hit testing (its texture is not readable).");
            return;
        }
        alphaHitTestMinimumThreshold_ = value;
    }

    void Image::SetUseSpriteMesh(bool value)
    {
        if (useSpriteMesh_ != value)
            useSpriteMesh_ = value, SetVerticesDirty();
    }

    bool Image::hasBorder() const
    {
        engine::Sprite* s = activeSprite();
        if (!s)
            return false;
        Vector4 b = s->border();
        return b.x * b.x + b.y * b.y + b.z * b.z + b.w * b.w > 0;
    }

    void Image::SetPixelsPerUnitMultiplier(f32 value)
    {
        pixelsPerUnitMultiplier_ = value > 0.01f ? value : 0.01f;
        SetVerticesDirty();
    }

    f32 Image::pixelsPerUnit()
    {
        if (engine::Canvas* c = canvas())
            cachedReferencePixelsPerUnit_ = c->referencePixelsPerUnit();
        return PixelsPerUnitConst();
    }

    f32 Image::PixelsPerUnitConst() const
    {
        engine::Sprite* s = activeSprite();
        f32 sprite = s ? s->pixelsPerUnit() : 100;
        return sprite / cachedReferencePixelsPerUnit_;
    }

    void Image::OnAfterDeserialize()
    {
        if (fillOrigin_ < 0)
            fillOrigin_ = 0;
        i32 max = fillMethod_ == FillMethod::Horizontal || fillMethod_ == FillMethod::Vertical ? 1 : 3;
        if (fillOrigin_ > max)
            fillOrigin_ = 0;
        fillAmount_ = Clamp01(fillAmount_);
    }

    engine::Texture* Image::mainTexture() const
    {
        engine::Sprite* s = activeSprite();
        if (!s)
        {
            engine::Material* m = material();
            if (m && m->mainTexture())
                return m->mainTexture();
            return engine::Texture2D::whiteTexture();
        }
        return s->texture();
    }

    engine::Material* Image::material() const
    {
        if (engine::Material* own = ownMaterial())
            return own;
        if (engine::Sprite* s = activeSprite(); s && s->associatedAlphaSplitTexture())
            return defaultETC1GraphicMaterial();
        return defaultMaterial();
    }

    void Image::SetNativeSize()
    {
        engine::Sprite* s = activeSprite();
        engine::RectTransform* rt = rectTransform();
        if (!s || !rt)
            return;
        f32 ppu = pixelsPerUnit();
        f32 w = s->rect().width / ppu, h = s->rect().height / ppu;
        rt->SetAnchorMax(rt->anchorMin());
        rt->SetSizeDelta(Vector2(w, h));
        SetAllDirty();
    }

    void Image::UpdateMaterial()
    {
        Graphic::UpdateMaterial();
        engine::CanvasRenderer* r = canvasRenderer();
        if (!r)
            return;
        engine::Sprite* s = activeSprite();
        if (!s)
        {
            r->SetAlphaTexture(nullptr);
            return;
        }
        if (engine::Texture2D* alpha = s->associatedAlphaSplitTexture())
            r->SetAlphaTexture(alpha);
    }

    f32 Image::preferredWidth() const
    {
        engine::Sprite* s = activeSprite();
        if (!s)
            return 0;
        f32 ppu = const_cast<Image*>(this)->pixelsPerUnit();
        if (type_ == Type::Sliced || type_ == Type::Tiled)
            return engine::DataUtility::GetMinSize(s).x / ppu;
        return s->rect().width / ppu;
    }

    f32 Image::preferredHeight() const
    {
        engine::Sprite* s = activeSprite();
        if (!s)
            return 0;
        f32 ppu = const_cast<Image*>(this)->pixelsPerUnit();
        if (type_ == Type::Sliced || type_ == Type::Tiled)
            return engine::DataUtility::GetMinSize(s).y / ppu;
        return s->rect().height / ppu;
    }

    // ----------------------------------------------------------------------------------------------- raycasts

    Vector2 Image::MapCoordinate(Vector2 local, Rect rect)
    {
        engine::Sprite* s = activeSprite();
        Rect spriteRect = s->rect();
        if (type_ == Type::Simple || type_ == Type::Filled)
            return Vector2(local.x * spriteRect.width / rect.width, local.y * spriteRect.height / rect.height);
        Vector4 border = s->border();
        f32 ppu = pixelsPerUnit();
        Vector4 adjusted = AdjustedBorders(Vector4(border.x / ppu, border.y / ppu, border.z / ppu, border.w / ppu), rect);
        Vector2 size(rect.width, rect.height), spriteSize(spriteRect.width, spriteRect.height);
        for (i32 i = 0; i < 2; i++)
        {
            if (Get(local, i) <= Get(adjusted, i))
                continue;
            if (Get(size, i) - Get(local, i) <= Get(adjusted, i + 2))
            {
                Put(local, i, Get(local, i) - (Get(size, i) - Get(spriteSize, i)));
                continue;
            }
            if (type_ == Type::Sliced)
            {
                f32 lo = Get(adjusted, i), hi = Get(size, i) - Get(adjusted, i + 2);
                f32 t = hi != lo ? (Get(local, i) - lo) / (hi - lo) : 0;
                t = Clamp01(t);
                Put(local, i, Lerp(Get(border, i), Get(spriteSize, i) - Get(border, i + 2), t));
            }
            else
            {
                f32 tile = Get(spriteSize, i) - Get(border, i) - Get(border, i + 2);
                f32 v = Get(local, i) - Get(adjusted, i);
                v = tile > 0 ? v - std::floor(v / tile) * tile : 0;
                Put(local, i, v + Get(border, i));
            }
        }
        return local;
    }

    bool Image::IsRaycastLocationValid(Vector2 screenPoint, engine::Camera* eventCamera)
    {
        if (alphaHitTestMinimumThreshold_ <= 0)
            return true;
        if (alphaHitTestMinimumThreshold_ > 1)
            return false;
        engine::Sprite* s = activeSprite();
        engine::RectTransform* rt = rectTransform();
        if (!s || !rt)
            return true;
        Vector2 local;
        if (!engine::RectTransformUtility::ScreenPointToLocalPointInRectangle(rt, screenPoint, eventCamera, local))
            return false;
        Rect rect = GetPixelAdjustedRect();
        local.x += rt->pivot().x * rect.width;
        local.y += rt->pivot().y * rect.height;
        local = MapCoordinate(local, rect);
        engine::Texture2D* texture = s->texture();
        if (!texture || !texture->isReadable())
        {
            HX_LOG_ERROR(CanvasUILog, "Using alphaHitTestMinimumThreshold greater than 0 on Image whose sprite texture cannot be read.");
            return true;
        }
        Rect tr = s->textureRect();
        f32 u = (tr.x + local.x) / f32(texture->width()), v = (tr.y + local.y) / f32(texture->height());
        return texture->GetPixelBilinear(u, v).a >= alphaHitTestMinimumThreshold_;
    }

    // ----------------------------------------------------------------------------------------------- the mesh

    void Image::OnPopulateMesh(VertexHelper* toFill)
    {
        if (!activeSprite())
        {
            Graphic::OnPopulateMesh(toFill);
            return;
        }
        switch (type_)
        {
        case Type::Simple:
            if (useSpriteMesh_)
                GenerateSpriteMesh(*toFill, preserveAspect_);
            else
                GenerateSimple(*toFill, preserveAspect_);
            break;
        case Type::Sliced: GenerateSliced(*toFill); break;
        case Type::Tiled: GenerateTiled(*toFill); break;
        case Type::Filled: GenerateFilled(*toFill, preserveAspect_); break;
        }
    }

    void Image::PreserveAspect(Rect& rect, Vector2 spriteSize) const
    {
        f32 spriteRatio = spriteSize.x / spriteSize.y, rectRatio = rect.width / rect.height;
        engine::RectTransform* rt = rectTransform();
        Vector2 pivot = rt ? rt->pivot() : Vector2(0.5f, 0.5f);
        if (spriteRatio > rectRatio)
        {
            f32 old = rect.height;
            rect.height = rect.width * (1.0f / spriteRatio);
            rect.y += (old - rect.height) * pivot.y;
        }
        else
        {
            f32 old = rect.width;
            rect.width = rect.height * spriteRatio;
            rect.x += (old - rect.width) * pivot.x;
        }
    }

    Vector4 Image::DrawingDimensions(bool preserveAspect)
    {
        engine::Sprite* s = activeSprite();
        Vector4 padding = s ? engine::DataUtility::GetPadding(s) : Vector4(0, 0, 0, 0);
        Vector2 size = s ? Vector2(s->rect().width, s->rect().height) : Vector2(0, 0);
        Rect r = GetPixelAdjustedRect();
        f32 w = std::round(size.x), h = std::round(size.y);
        Vector4 v(w != 0 ? padding.x / w : 0, h != 0 ? padding.y / h : 0, w != 0 ? (w - padding.z) / w : 0, h != 0 ? (h - padding.w) / h : 0);
        if (preserveAspect && size.x * size.x + size.y * size.y > 0)
            PreserveAspect(r, size);
        return Vector4(r.x + r.width * v.x, r.y + r.height * v.y, r.x + r.width * v.z, r.y + r.height * v.w);
    }

    Vector4 Image::AdjustedBorders(Vector4 border, Rect adjusted) const
    {
        engine::RectTransform* rt = rectTransform();
        Rect original = rt ? rt->rect() : adjusted;
        Vector2 originalSize(original.width, original.height), adjustedSize(adjusted.width, adjusted.height);
        for (i32 axis = 0; axis <= 1; axis++)
        {
            // A pixel adjusted rectangle a little larger than the original: the borders follow it (no gaps).
            if (Get(originalSize, axis) != 0)
            {
                f32 ratio = Get(adjustedSize, axis) / Get(originalSize, axis);
                Put(border, axis, Get(border, axis) * ratio);
                Put(border, axis + 2, Get(border, axis + 2) * ratio);
            }
            // Less room than the borders: they shrink to fit (no overlaps).
            f32 combined = Get(border, axis) + Get(border, axis + 2);
            if (Get(adjustedSize, axis) < combined && combined != 0)
            {
                f32 ratio = Get(adjustedSize, axis) / combined;
                Put(border, axis, Get(border, axis) * ratio);
                Put(border, axis + 2, Get(border, axis + 2) * ratio);
            }
        }
        return border;
    }

    void Image::GenerateSimple(VertexHelper& vh, bool preserveAspect)
    {
        Vector4 v = DrawingDimensions(preserveAspect);
        engine::Sprite* s = activeSprite();
        Vector4 uv = s ? engine::DataUtility::GetOuterUV(s) : Vector4(0, 0, 0, 0);
        Color32 c = color();
        vh.Clear();
        vh.AddVert(Vector3(v.x, v.y, 0), c, Vector4(uv.x, uv.y, 0, 0));
        vh.AddVert(Vector3(v.x, v.w, 0), c, Vector4(uv.x, uv.w, 0, 0));
        vh.AddVert(Vector3(v.z, v.w, 0), c, Vector4(uv.z, uv.w, 0, 0));
        vh.AddVert(Vector3(v.z, v.y, 0), c, Vector4(uv.z, uv.y, 0, 0));
        vh.AddTriangle(0, 1, 2);
        vh.AddTriangle(2, 3, 0);
    }

    void Image::GenerateSpriteMesh(VertexHelper& vh, bool preserveAspect)
    {
        engine::Sprite* s = activeSprite();
        engine::RectTransform* rt = rectTransform();
        Vector2 spriteSize(s->rect().width, s->rect().height);
        Vector2 spritePivot(s->pivot().x / spriteSize.x, s->pivot().y / spriteSize.y);
        Vector2 rectPivot = rt ? rt->pivot() : Vector2(0.5f, 0.5f);
        Rect r = GetPixelAdjustedRect();
        if (preserveAspect && spriteSize.x * spriteSize.x + spriteSize.y * spriteSize.y > 0)
            PreserveAspect(r, spriteSize);
        Vector2 drawing(r.width, r.height);
        Vector3 bounds = s->bounds().size();
        Vector2 offset((rectPivot.x - spritePivot.x) * drawing.x, (rectPivot.y - spritePivot.y) * drawing.y);
        Color32 c = color();
        vh.Clear();
        Array<Vector2> vertices = s->vertices();
        Array<Vector2> uvs = s->uv();
        for (usize i = 0; i < vertices.Size(); i++)
            vh.AddVert(Vector3(vertices[i].x / bounds.x * drawing.x - offset.x, vertices[i].y / bounds.y * drawing.y - offset.y, 0), c,
                       Vector4(uvs[i].x, uvs[i].y, 0, 0));
        Array<u16> triangles = s->triangles();
        for (usize i = 0; i + 2 < triangles.Size(); i += 3)
            vh.AddTriangle(triangles[i], triangles[i + 1], triangles[i + 2]);
    }

    void Image::GenerateSliced(VertexHelper& vh)
    {
        if (!hasBorder())
        {
            GenerateSimple(vh, false);
            return;
        }
        engine::Sprite* s = activeSprite();
        Vector4 outer = engine::DataUtility::GetOuterUV(s), inner = engine::DataUtility::GetInnerUV(s);
        Vector4 padding = engine::DataUtility::GetPadding(s), border = s->border();
        Rect rect = GetPixelAdjustedRect();
        f32 ppu = multipliedPixelsPerUnit();
        Vector4 adjusted = AdjustedBorders(Vector4(border.x / ppu, border.y / ppu, border.z / ppu, border.w / ppu), rect);
        padding = Vector4(padding.x / ppu, padding.y / ppu, padding.z / ppu, padding.w / ppu);
        Vector2 vert[4] = {Vector2(padding.x, padding.y), Vector2(adjusted.x, adjusted.y), Vector2(rect.width - adjusted.z, rect.height - adjusted.w),
                           Vector2(rect.width - padding.z, rect.height - padding.w)};
        for (Vector2& p : vert)
            p.x += rect.x, p.y += rect.y;
        Vector2 uv[4] = {Vector2(outer.x, outer.y), Vector2(inner.x, inner.y), Vector2(inner.z, inner.w), Vector2(outer.z, outer.w)};
        vh.Clear();
        Color32 c = color();
        for (i32 x = 0; x < 3; x++)
            for (i32 y = 0; y < 3; y++)
            {
                if (!fillCenter_ && x == 1 && y == 1)
                    continue;
                // No quads of no size.
                if (vert[x + 1].x - vert[x].x <= 0 || vert[y + 1].y - vert[y].y <= 0)
                    continue;
                Quad(vh, Vector2(vert[x].x, vert[y].y), Vector2(vert[x + 1].x, vert[y + 1].y), c, Vector2(uv[x].x, uv[y].y), Vector2(uv[x + 1].x, uv[y + 1].y));
            }
    }

    void Image::GenerateTiled(VertexHelper& vh)
    {
        engine::Sprite* s = activeSprite();
        Vector4 outer = s ? engine::DataUtility::GetOuterUV(s) : Vector4(0, 0, 0, 0);
        Vector4 inner = s ? engine::DataUtility::GetInnerUV(s) : Vector4(0, 0, 0, 0);
        Vector4 border = s ? s->border() : Vector4(0, 0, 0, 0);
        Vector2 spriteSize = s ? Vector2(s->rect().width, s->rect().height) : Vector2(100, 100);
        Rect rect = GetPixelAdjustedRect();
        f32 ppu = multipliedPixelsPerUnit();
        f32 tileWidth = (spriteSize.x - border.x - border.z) / ppu, tileHeight = (spriteSize.y - border.y - border.w) / ppu;
        border = AdjustedBorders(Vector4(border.x / ppu, border.y / ppu, border.z / ppu, border.w / ppu), rect);
        Vector2 uvMin(inner.x, inner.y), uvMax(inner.z, inner.w);
        f32 xMin = border.x, xMax = rect.width - border.z, yMin = border.y, yMax = rect.height - border.w;
        vh.Clear();
        Vector2 clipped = uvMax;
        if (tileWidth <= 0)
            tileWidth = xMax - xMin;
        if (tileHeight <= 0)
            tileHeight = yMax - yMin;
        Color32 c = color();
        Vector2 at(rect.x, rect.y);
        auto Q = [&](Vector2 a, Vector2 b, Vector2 u0, Vector2 u1) { Quad(vh, Vector2(a.x + at.x, a.y + at.y), Vector2(b.x + at.x, b.y + at.y), c, u0, u1); };
        bool bordered = hasBorder();
        bool repeats = s && s->texture() && s->texture()->wrapMode() == engine::TextureWrapMode::Repeat;
        if (s && (bordered || s->packed() || !repeats))
        {
            // Quads per tile (the texture cannot repeat by itself here); at most 65000 vertices.
            i64 tilesW = 0, tilesH = 0;
            if (fillCenter_)
            {
                tilesW = i64(std::ceil((xMax - xMin) / tileWidth));
                tilesH = i64(std::ceil((yMax - yMin) / tileHeight));
                f64 vertices = bordered ? (f64(tilesW) + 2.0) * (f64(tilesH) + 2.0) * 4.0 : f64(tilesW) * f64(tilesH) * 4.0;
                if (vertices > 65000.0)
                {
                    HX_LOG_ERROR(CanvasUILog, "Too many sprite tiles on Image \"{}\". The tile size will be increased. To remove the limit on the number of tiles, set the Wrap mode to Repeat in the Image Import Settings",
                                 Name());
                    f64 maxTiles = 65000.0 / 4.0;
                    f64 ratio = bordered ? (f64(tilesW) + 2.0) / (f64(tilesH) + 2.0) : f64(tilesW) / f64(tilesH);
                    f64 targetW = std::sqrt(maxTiles / ratio), targetH = targetW * ratio;
                    if (bordered)
                        targetW -= 2, targetH -= 2;
                    tilesW = i64(std::floor(targetW)), tilesH = i64(std::floor(targetH));
                    tileWidth = (xMax - xMin) / f32(tilesW), tileHeight = (yMax - yMin) / f32(tilesH);
                }
            }
            else if (bordered)
            {
                tilesW = i64(std::ceil((xMax - xMin) / tileWidth));
                tilesH = i64(std::ceil((yMax - yMin) / tileHeight));
                f64 vertices = (f64(tilesH) + f64(tilesW) + 2.0) * 2.0 * 4.0;
                if (vertices > 65000.0)
                {
                    HX_LOG_ERROR(CanvasUILog, "Too many sprite tiles on Image \"{}\". The tile size will be increased. To remove the limit on the number of tiles, set the Wrap mode to Repeat in the Image Import Settings",
                                 Name());
                    f64 maxTiles = 65000.0 / 4.0, ratio = f64(tilesW) / f64(tilesH);
                    f64 targetW = (maxTiles - 4) / (2 * (1.0 + ratio)), targetH = targetW * ratio;
                    tilesW = i64(std::floor(targetW)), tilesH = i64(std::floor(targetH));
                    tileWidth = (xMax - xMin) / f32(tilesW), tileHeight = (yMax - yMin) / f32(tilesH);
                }
            }
            if (fillCenter_)
                for (i64 j = 0; j < tilesH; j++)
                {
                    f32 y1 = yMin + f32(j) * tileHeight, y2 = yMin + f32(j + 1) * tileHeight;
                    if (y2 > yMax)
                        clipped.y = uvMin.y + (uvMax.y - uvMin.y) * (yMax - y1) / (y2 - y1), y2 = yMax;
                    clipped.x = uvMax.x;
                    for (i64 i = 0; i < tilesW; i++)
                    {
                        f32 x1 = xMin + f32(i) * tileWidth, x2 = xMin + f32(i + 1) * tileWidth;
                        if (x2 > xMax)
                            clipped.x = uvMin.x + (uvMax.x - uvMin.x) * (xMax - x1) / (x2 - x1), x2 = xMax;
                        Q(Vector2(x1, y1), Vector2(x2, y2), uvMin, clipped);
                    }
                }
            if (bordered)
            {
                clipped = uvMax;
                for (i64 j = 0; j < tilesH; j++)
                {
                    f32 y1 = yMin + f32(j) * tileHeight, y2 = yMin + f32(j + 1) * tileHeight;
                    if (y2 > yMax)
                        clipped.y = uvMin.y + (uvMax.y - uvMin.y) * (yMax - y1) / (y2 - y1), y2 = yMax;
                    Q(Vector2(0, y1), Vector2(xMin, y2), Vector2(outer.x, uvMin.y), Vector2(uvMin.x, clipped.y));
                    Q(Vector2(xMax, y1), Vector2(rect.width, y2), Vector2(uvMax.x, uvMin.y), Vector2(outer.z, clipped.y));
                }
                clipped = uvMax;
                for (i64 i = 0; i < tilesW; i++)
                {
                    f32 x1 = xMin + f32(i) * tileWidth, x2 = xMin + f32(i + 1) * tileWidth;
                    if (x2 > xMax)
                        clipped.x = uvMin.x + (uvMax.x - uvMin.x) * (xMax - x1) / (x2 - x1), x2 = xMax;
                    Q(Vector2(x1, 0), Vector2(x2, yMin), Vector2(uvMin.x, outer.y), Vector2(clipped.x, uvMin.y));
                    Q(Vector2(x1, yMax), Vector2(x2, rect.height), Vector2(uvMin.x, uvMax.y), Vector2(clipped.x, outer.w));
                }
                Q(Vector2(0, 0), Vector2(xMin, yMin), Vector2(outer.x, outer.y), Vector2(uvMin.x, uvMin.y));
                Q(Vector2(xMax, 0), Vector2(rect.width, yMin), Vector2(uvMax.x, outer.y), Vector2(outer.z, uvMin.y));
                Q(Vector2(0, yMax), Vector2(xMin, rect.height), Vector2(outer.x, uvMax.y), Vector2(uvMin.x, outer.w));
                Q(Vector2(xMax, yMax), Vector2(rect.width, rect.height), Vector2(uvMax.x, uvMax.y), Vector2(outer.z, outer.w));
            }
        }
        else
        {
            // The texture repeats by its uvs: one quad.
            Vector2 scale((xMax - xMin) / tileWidth, (yMax - yMin) / tileHeight);
            if (fillCenter_)
                Q(Vector2(xMin, yMin), Vector2(xMax, yMax), Vector2(uvMin.x * scale.x, uvMin.y * scale.y), Vector2(uvMax.x * scale.x, uvMax.y * scale.y));
        }
    }

    void Image::GenerateFilled(VertexHelper& vh, bool preserveAspect)
    {
        vh.Clear();
        if (fillAmount_ < 0.001f)
            return;
        Vector4 v = DrawingDimensions(preserveAspect);
        engine::Sprite* s = activeSprite();
        Vector4 outer = s ? engine::DataUtility::GetOuterUV(s) : Vector4(0, 0, 0, 0);
        Color32 c = color();
        f32 tx0 = outer.x, ty0 = outer.y, tx1 = outer.z, ty1 = outer.w;
        if (fillMethod_ == FillMethod::Horizontal)
        {
            f32 fill = (tx1 - tx0) * fillAmount_;
            if (fillOrigin_ == 1)
                v.x = v.z - (v.z - v.x) * fillAmount_, tx0 = tx1 - fill;
            else
                v.z = v.x + (v.z - v.x) * fillAmount_, tx1 = tx0 + fill;
        }
        else if (fillMethod_ == FillMethod::Vertical)
        {
            f32 fill = (ty1 - ty0) * fillAmount_;
            if (fillOrigin_ == 1)
                v.y = v.w - (v.w - v.y) * fillAmount_, ty0 = ty1 - fill;
            else
                v.w = v.y + (v.w - v.y) * fillAmount_, ty1 = ty0 + fill;
        }
        Vector2 xy[4] = {Vector2(v.x, v.y), Vector2(v.x, v.w), Vector2(v.z, v.w), Vector2(v.z, v.y)};
        Vector2 uv[4] = {Vector2(tx0, ty0), Vector2(tx0, ty1), Vector2(tx1, ty1), Vector2(tx1, ty0)};
        bool radial = fillMethod_ != FillMethod::Horizontal && fillMethod_ != FillMethod::Vertical;
        if (fillAmount_ >= 1 || !radial)
        {
            Quad(vh, xy, c, uv);
            return;
        }
        // A part of the drawing rectangle and of the uvs (fractions fx0..fx1, fy0..fy1).
        auto part = [&](f32 fx0, f32 fx1, f32 fy0, f32 fy1) {
            xy[0].x = xy[1].x = Lerp(v.x, v.z, fx0);
            xy[2].x = xy[3].x = Lerp(v.x, v.z, fx1);
            xy[0].y = xy[3].y = Lerp(v.y, v.w, fy0);
            xy[1].y = xy[2].y = Lerp(v.y, v.w, fy1);
            uv[0].x = uv[1].x = Lerp(tx0, tx1, fx0);
            uv[2].x = uv[3].x = Lerp(tx0, tx1, fx1);
            uv[0].y = uv[3].y = Lerp(ty0, ty1, fy0);
            uv[1].y = uv[2].y = Lerp(ty0, ty1, fy1);
        };
        if (fillMethod_ == FillMethod::Radial90)
        {
            if (RadialCut(xy, uv, fillAmount_, fillClockwise_, fillOrigin_))
                Quad(vh, xy, c, uv);
        }
        else if (fillMethod_ == FillMethod::Radial180)
        {
            for (i32 side = 0; side < 2; side++)
            {
                i32 even = fillOrigin_ > 1 ? 1 : 0;
                if (fillOrigin_ == 0 || fillOrigin_ == 2)
                    part(side == even ? 0.0f : 0.5f, side == even ? 0.5f : 1.0f, 0, 1);
                else
                    part(0, 1, side == even ? 0.5f : 0.0f, side == even ? 1.0f : 0.5f);
                f32 val = fillClockwise_ ? fillAmount_ * 2 - f32(side) : fillAmount_ * 2 - f32(1 - side);
                if (RadialCut(xy, uv, Clamp01(val), fillClockwise_, (side + fillOrigin_ + 3) % 4))
                    Quad(vh, xy, c, uv);
            }
        }
        else
        {
            for (i32 corner = 0; corner < 4; corner++)
            {
                part(corner < 2 ? 0.0f : 0.5f, corner < 2 ? 0.5f : 1.0f, corner == 0 || corner == 3 ? 0.0f : 0.5f, corner == 0 || corner == 3 ? 0.5f : 1.0f);
                f32 val = fillClockwise_ ? fillAmount_ * 4 - f32((corner + fillOrigin_) % 4) : fillAmount_ * 4 - f32(3 - ((corner + fillOrigin_) % 4));
                if (RadialCut(xy, uv, Clamp01(val), fillClockwise_, (corner + 2) % 4))
                    Quad(vh, xy, c, uv);
            }
        }
    }

    // ----------------------------------------------------------------------------------------------- messages

    void Image::OnEnable() { MaskableGraphic::OnEnable(); }
    void Image::OnDisable() { MaskableGraphic::OnDisable(); }

    void Image::OnCanvasHierarchyChanged()
    {
        MaskableGraphic::OnCanvasHierarchyChanged();
        if (engine::Canvas* c = canvas(); c && c->referencePixelsPerUnit() != cachedReferencePixelsPerUnit_)
        {
            cachedReferencePixelsPerUnit_ = c->referencePixelsPerUnit();
            if (type_ == Type::Sliced || type_ == Type::Tiled)
            {
                SetVerticesDirty();
                SetLayoutDirty();
            }
        }
    }

    void Image::OnDidApplyAnimationProperties()
    {
        SetMaterialDirty();
        SetVerticesDirty();
        SetRaycastDirty();
    }

    // ------------------------------------------------------------------------------------------------- RawImage

    void RawImage::SetTexture(engine::Texture* value)
    {
        if (texture_.Get() == value)
            return;
        texture_ = engine::PPtr<engine::Texture>(value);
        SetVerticesDirty();
        SetMaterialDirty();
    }

    void RawImage::SetUvRect(Rect value)
    {
        if (uvRect_.x == value.x && uvRect_.y == value.y && uvRect_.width == value.width && uvRect_.height == value.height)
            return;
        uvRect_ = value;
        SetVerticesDirty();
    }

    engine::Texture* RawImage::mainTexture() const
    {
        if (engine::Texture* t = texture_.Get())
            return t;
        if (engine::Material* m = material(); m && m->mainTexture())
            return m->mainTexture();
        return engine::Texture2D::whiteTexture();
    }

    void RawImage::SetNativeSize()
    {
        engine::Texture* t = mainTexture();
        engine::RectTransform* rt = rectTransform();
        if (!t || !rt)
            return;
        i32 w = i32(std::round(f32(t->width()) * uvRect_.width)), h = i32(std::round(f32(t->height()) * uvRect_.height));
        rt->SetAnchorMax(rt->anchorMin());
        rt->SetSizeDelta(Vector2(f32(w), f32(h)));
    }

    void RawImage::OnPopulateMesh(VertexHelper* vh)
    {
        engine::Texture* t = mainTexture();
        vh->Clear();
        if (!t)
            return;
        Rect r = GetPixelAdjustedRect();
        Vector4 v(r.x, r.y, r.x + r.width, r.y + r.height);
        Color32 c = color();
        Rect u = uvRect_;
        vh->AddVert(Vector3(v.x, v.y, 0), c, Vector4(u.xMin(), u.yMin(), 0, 0));
        vh->AddVert(Vector3(v.x, v.w, 0), c, Vector4(u.xMin(), u.yMax(), 0, 0));
        vh->AddVert(Vector3(v.z, v.w, 0), c, Vector4(u.xMax(), u.yMax(), 0, 0));
        vh->AddVert(Vector3(v.z, v.y, 0), c, Vector4(u.xMax(), u.yMin(), 0, 0));
        vh->AddTriangle(0, 1, 2);
        vh->AddTriangle(2, 3, 0);
    }

    void RawImage::OnDidApplyAnimationProperties()
    {
        SetMaterialDirty();
        SetVerticesDirty();
        SetRaycastDirty();
    }
}
