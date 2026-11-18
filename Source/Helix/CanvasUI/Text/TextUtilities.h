#pragma once

#include <Helix/CanvasUI/Text/TextBase.h>
#include <Helix/Engine/Rendering/Camera.h>

// Where a screen point is in a text: the character, word, line or link under it (or nearest to it), the caret's
// index between characters; points of the screen on a RectTransform's plane. Positions are screen pixels (the camera
// null for an overlay canvas).
namespace helix::canvasui
{
    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class CaretPosition : i32
    {
        None = 0,
        Left = 1,
        Right = 2,
    };

    HX_CLASS(Namespace = "HelixEngine.Text", Static)
    class TextUtilities
    {
    public:
        HX_FUNCTION() static i32 FindIntersectingCharacter(TextBase* text, Vector3 position, engine::Camera* camera, bool visibleOnly);
        HX_FUNCTION() static i32 FindNearestCharacter(TextBase* text, Vector3 position, engine::Camera* camera, bool visibleOnly);
        HX_FUNCTION() static i32 FindNearestCharacterOnLine(TextBase* text, Vector3 position, i32 line, engine::Camera* camera, bool visibleOnly);
        HX_FUNCTION() static i32 FindIntersectingWord(TextBase* text, Vector3 position, engine::Camera* camera);
        HX_FUNCTION() static i32 FindNearestWord(TextBase* text, Vector3 position, engine::Camera* camera);
        HX_FUNCTION() static i32 FindIntersectingLine(TextBase* text, Vector3 position, engine::Camera* camera);
        HX_FUNCTION() static i32 FindNearestLine(TextBase* text, Vector3 position, engine::Camera* camera);
        HX_FUNCTION() static i32 FindIntersectingLink(TextBase* text, Vector3 position, engine::Camera* camera);
        HX_FUNCTION() static i32 FindNearestLink(TextBase* text, Vector3 position, engine::Camera* camera);
        // The caret's index for the point: before the character nearest it (after it when the point is right of its
        // middle).
        HX_FUNCTION() static i32 GetCursorIndexFromPosition(TextBase* textComponent, Vector3 position, engine::Camera* camera);
        HX_FUNCTION(Out = "cursor") static i32 GetCursorIndexFromPosition(TextBase* textComponent, Vector3 position, engine::Camera* camera, CaretPosition& cursor);
        HX_FUNCTION() static bool IsIntersectingRectTransform(engine::RectTransform* rectTransform, Vector3 position, engine::Camera* camera);
        HX_FUNCTION(Out = "worldPoint") static bool ScreenPointToWorldPointInRectangle(engine::Transform* transform, Vector2 screenPoint, engine::Camera* cam,
                                                                                     Vector3& worldPoint);
        // The distance of a point from the segment a to b.
        HX_FUNCTION() static f32 DistanceToLine(Vector3 a, Vector3 b, Vector3 point);

        // The point in the text's local space (false when the ray misses its plane).
        static bool LocalPoint(TextBase* text, Vector3 position, engine::Camera* camera, Vector3& local);
    };
}
