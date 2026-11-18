#include <Helix/CanvasUI/Text/TextUtilities.h>

#include <Helix/Engine/Transform/RectTransform.h>

#include <cmath>
#include <limits>

namespace helix::canvasui
{
    namespace
    {
        // Whether p is inside the quad (corners in order around it).
        bool InQuad(Vector3 p, Vector3 bl, Vector3 tl, Vector3 tr, Vector3 br)
        {
            const Vector3 c[4] = {bl, tl, tr, br};
            bool pos = false, neg = false;
            for (int i = 0; i < 4; i++)
            {
                Vector3 a = c[i], b = c[(i + 1) % 4];
                f32 cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
                pos = pos || cross > 0;
                neg = neg || cross < 0;
            }
            return !(pos && neg);
        }

        // The distance from p to the quad (0 inside).
        f32 ToQuad(Vector3 p, Vector3 bl, Vector3 tl, Vector3 tr, Vector3 br)
        {
            if (InQuad(p, bl, tl, tr, br))
                return 0;
            f32 d = TextUtilities::DistanceToLine(bl, tl, p);
            d = std::fmin(d, TextUtilities::DistanceToLine(tl, tr, p));
            d = std::fmin(d, TextUtilities::DistanceToLine(tr, br, p));
            return std::fmin(d, TextUtilities::DistanceToLine(br, bl, p));
        }

        // A character's box: its glyph's, a white space's from its origin to its advance between its line's ascender and descender.
        void Box(const CharacterInfo& c, Vector3& bl, Vector3& tl, Vector3& tr, Vector3& br)
        {
            if (c.isVisible)
            {
                bl = c.bottomLeft, tl = c.topLeft, tr = c.topRight, br = c.bottomRight;
                return;
            }
            bl = Vector3(c.origin, c.descender, 0), tl = Vector3(c.origin, c.ascender, 0);
            tr = Vector3(c.xAdvance, c.ascender, 0), br = Vector3(c.xAdvance, c.descender, 0);
        }

        // A character's box spanning its line (words and links are tested on whole line heights).
        void LineBox(const TextInfo& info, const CharacterInfo& c, Vector3& bl, Vector3& tl, Vector3& tr, Vector3& br)
        {
            f32 top = c.ascender, bottom = c.descender;
            if (c.lineNumber >= 0 && usize(c.lineNumber) < info.lineInfo.Size())
                top = info.lineInfo[usize(c.lineNumber)].ascender, bottom = info.lineInfo[usize(c.lineNumber)].descender;
            bl = Vector3(c.origin, bottom, 0), tl = Vector3(c.origin, top, 0);
            tr = Vector3(c.xAdvance, top, 0), br = Vector3(c.xAdvance, bottom, 0);
        }
    }

    bool TextUtilities::LocalPoint(TextBase* text, Vector3 position, engine::Camera* camera, Vector3& local)
    {
        if (!text)
            return false;
        engine::Transform* t = text->engine::Component::transform();
        Vector3 world;
        if (!ScreenPointToWorldPointInRectangle(t, Vector2(position.x, position.y), camera, world))
            return false;
        local = t->InverseTransformPoint(world);
        return true;
    }

    bool TextUtilities::ScreenPointToWorldPointInRectangle(engine::Transform* transform, Vector2 screenPoint, engine::Camera* cam, Vector3& worldPoint)
    {
        worldPoint = Vector3(0, 0, 0);
        if (!transform)
            return false;
        Vector3 origin = transform->position();
        Vector3 normal = transform->rotation() * Vector3(0, 0, -1);
        if (!cam)
        {
            // An overlay: the screen point on the plane straight along z.
            Vector3 p(screenPoint.x, screenPoint.y, 0);
            f32 nz = normal.z;
            if (std::fabs(nz) < 1e-6f)
                return false;
            p.z = origin.z - (normal.x * (p.x - origin.x) + normal.y * (p.y - origin.y)) / nz;
            worldPoint = p;
            return true;
        }
        Ray ray = cam->ScreenPointToRay(Vector3(screenPoint.x, screenPoint.y, 0));
        f32 denom = Vector3::Dot(normal, ray.direction);
        if (std::fabs(denom) < 1e-6f)
            return false;
        f32 d = Vector3::Dot(normal, origin - ray.origin) / denom;
        worldPoint = ray.origin + ray.direction * d;
        return true;
    }

    bool TextUtilities::IsIntersectingRectTransform(engine::RectTransform* rectTransform, Vector3 position, engine::Camera* camera)
    {
        if (!rectTransform)
            return false;
        Vector3 world;
        if (!ScreenPointToWorldPointInRectangle(rectTransform, Vector2(position.x, position.y), camera, world))
            return false;
        Vector3 corners[4];
        rectTransform->GetWorldCorners(corners);
        return InQuad(world, corners[0], corners[1], corners[2], corners[3]);
    }

    f32 TextUtilities::DistanceToLine(Vector3 a, Vector3 b, Vector3 point)
    {
        Vector3 ab = b - a, ap = point - a;
        f32 len = Vector3::Dot(ab, ab);
        f32 t = len > 0 ? Vector3::Dot(ap, ab) / len : 0;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        Vector3 c = a + ab * t;
        Vector3 d = point - c;
        return std::sqrt(Vector3::Dot(d, d));
    }

    i32 TextUtilities::FindIntersectingCharacter(TextBase* text, Vector3 position, engine::Camera* camera, bool visibleOnly)
    {
        Vector3 p;
        if (!LocalPoint(text, position, camera, p))
            return -1;
        const TextInfo& info = *text->textInfo();
        for (usize i = 0; i < info.characterInfo.Size(); i++)
        {
            const CharacterInfo& c = info.characterInfo[i];
            if (visibleOnly && !c.isVisible)
                continue;
            Vector3 bl, tl, tr, br;
            Box(c, bl, tl, tr, br);
            if (InQuad(p, bl, tl, tr, br))
                return i32(i);
        }
        return -1;
    }

    i32 TextUtilities::FindNearestCharacter(TextBase* text, Vector3 position, engine::Camera* camera, bool visibleOnly)
    {
        Vector3 p;
        if (!LocalPoint(text, position, camera, p))
            return -1;
        const TextInfo& info = *text->textInfo();
        f32 best = std::numeric_limits<f32>::max();
        i32 at = -1;
        for (usize i = 0; i < info.characterInfo.Size(); i++)
        {
            const CharacterInfo& c = info.characterInfo[i];
            if (visibleOnly && !c.isVisible)
                continue;
            Vector3 bl, tl, tr, br;
            Box(c, bl, tl, tr, br);
            f32 d = ToQuad(p, bl, tl, tr, br);
            if (d < best)
                best = d, at = i32(i);
        }
        return at;
    }

    i32 TextUtilities::FindNearestCharacterOnLine(TextBase* text, Vector3 position, i32 line, engine::Camera* camera, bool visibleOnly)
    {
        Vector3 p;
        if (!LocalPoint(text, position, camera, p))
            return -1;
        const TextInfo& info = *text->textInfo();
        if (line < 0 || usize(line) >= info.lineInfo.Size())
            return -1;
        const LineInfo& l = info.lineInfo[usize(line)];
        f32 best = std::numeric_limits<f32>::max();
        i32 at = -1;
        for (i32 i = l.firstCharacterIndex; i <= l.lastCharacterIndex && usize(i) < info.characterInfo.Size(); i++)
        {
            const CharacterInfo& c = info.characterInfo[usize(i)];
            if (visibleOnly && !c.isVisible)
                continue;
            Vector3 bl, tl, tr, br;
            LineBox(info, c, bl, tl, tr, br);
            f32 d = ToQuad(p, bl, tl, tr, br);
            if (d < best)
                best = d, at = i;
        }
        return at;
    }

    namespace
    {
        // The nearest (or intersecting, at distance 0) of ranges of characters on their line boxes.
        template <class Ranges>
        i32 FindRange(TextBase* text, Vector3 position, engine::Camera* camera, bool intersecting, Ranges ranges)
        {
            Vector3 p;
            if (!TextUtilities::LocalPoint(text, position, camera, p))
                return -1;
            const TextInfo& info = *text->textInfo();
            f32 best = std::numeric_limits<f32>::max();
            i32 at = -1;
            ranges(info, [&](i32 index, i32 first, i32 count) {
                for (i32 k = first; k < first + count && k >= 0 && usize(k) < info.characterInfo.Size(); k++)
                {
                    Vector3 bl, tl, tr, br;
                    LineBox(info, info.characterInfo[usize(k)], bl, tl, tr, br);
                    f32 d = ToQuad(p, bl, tl, tr, br);
                    if (d < best)
                        best = d, at = index;
                }
            });
            return intersecting && best > 0 ? -1 : at;
        }

        auto Words = [](const TextInfo& info, auto each) {
            for (usize w = 0; w < info.wordInfo.Size(); w++)
                each(i32(w), info.wordInfo[w].firstCharacterIndex, info.wordInfo[w].characterCount);
        };
        auto Links = [](const TextInfo& info, auto each) {
            for (usize l = 0; l < info.linkInfo.Size(); l++)
                each(i32(l), info.linkInfo[l].linkTextfirstCharacterIndex, info.linkInfo[l].linkTextLength);
        };
    }

    i32 TextUtilities::FindIntersectingWord(TextBase* text, Vector3 position, engine::Camera* camera) { return FindRange(text, position, camera, true, Words); }
    i32 TextUtilities::FindNearestWord(TextBase* text, Vector3 position, engine::Camera* camera) { return FindRange(text, position, camera, false, Words); }
    i32 TextUtilities::FindIntersectingLink(TextBase* text, Vector3 position, engine::Camera* camera) { return FindRange(text, position, camera, true, Links); }
    i32 TextUtilities::FindNearestLink(TextBase* text, Vector3 position, engine::Camera* camera) { return FindRange(text, position, camera, false, Links); }

    i32 TextUtilities::FindIntersectingLine(TextBase* text, Vector3 position, engine::Camera* camera)
    {
        Vector3 p;
        if (!LocalPoint(text, position, camera, p))
            return -1;
        const TextInfo& info = *text->textInfo();
        for (usize l = 0; l < info.lineInfo.Size(); l++)
            if (p.y <= info.lineInfo[l].ascender && p.y >= info.lineInfo[l].descender)
                return i32(l);
        return -1;
    }

    i32 TextUtilities::FindNearestLine(TextBase* text, Vector3 position, engine::Camera* camera)
    {
        Vector3 p;
        if (!LocalPoint(text, position, camera, p))
            return -1;
        const TextInfo& info = *text->textInfo();
        f32 best = std::numeric_limits<f32>::max();
        i32 at = -1;
        for (usize l = 0; l < info.lineInfo.Size(); l++)
        {
            const LineInfo& li = info.lineInfo[l];
            f32 d = p.y > li.ascender ? p.y - li.ascender : p.y < li.descender ? li.descender - p.y : 0;
            if (d < best)
                best = d, at = i32(l);
        }
        return at;
    }

    i32 TextUtilities::GetCursorIndexFromPosition(TextBase* textComponent, Vector3 position, engine::Camera* camera)
    {
        CaretPosition cursor;
        i32 index = GetCursorIndexFromPosition(textComponent, position, camera, cursor);
        return cursor == CaretPosition::Right ? index + 1 : index;
    }

    i32 TextUtilities::GetCursorIndexFromPosition(TextBase* textComponent, Vector3 position, engine::Camera* camera, CaretPosition& cursor)
    {
        cursor = CaretPosition::None;
        i32 line = FindNearestLine(textComponent, position, camera);
        if (line < 0)
            return 0;
        i32 index = FindNearestCharacterOnLine(textComponent, position, line, camera, false);
        if (index < 0)
            return 0;
        Vector3 p;
        LocalPoint(textComponent, position, camera, p);
        const CharacterInfo& c = textComponent->textInfo()->characterInfo[usize(index)];
        cursor = p.x < (c.origin + c.xAdvance) * 0.5f ? CaretPosition::Left : CaretPosition::Right;
        return index;
    }
}
