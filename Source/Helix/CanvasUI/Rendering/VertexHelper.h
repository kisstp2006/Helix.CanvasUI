#pragma once

#include <Helix/CanvasUI/Core/OwnedRoot.h>
#include <Helix/Engine/Canvas/CanvasRenderer.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Texture/Texture.h>

namespace helix::engine
{
    class Mesh;
}

// VertexHelper: the vertices and triangles a graphic builds its mesh of (positions, colors, four texture
// coordinates, normals and tangents per vertex), filled into a mesh for the CanvasRenderer.
namespace helix::canvasui
{
    HX_CLASS(Namespace = "HelixEngine.CanvasUI", Root, Owned)
    class VertexHelper : public OwnedRoot<VertexHelper>
    {
    public:
        HX_FUNCTION(Constructor) VertexHelper() = default;
        // The mesh's vertices and triangles taken in.
        HX_FUNCTION(Constructor) explicit VertexHelper(engine::Mesh* m);
        ~VertexHelper() override = default;

        HX_FUNCTION() void Dispose() { Clear(); }
        HX_FUNCTION() void Clear()
        {
            vertices_.Clear();
            indices_.Clear();
        }
        HX_PROPERTY(ReadOnly) i32 currentVertCount() const { return i32(vertices_.Size()); }
        HX_PROPERTY(ReadOnly) i32 currentIndexCount() const { return i32(indices_.Size()); }
        HX_FUNCTION() void PopulateUIVertex(engine::UIVertex& vertex, i32 i) const;
        HX_FUNCTION() void SetUIVertex(engine::UIVertex vertex, i32 i);
        // Into the mesh (cleared first): positions, colors, uv0 to uv3, normals, tangents and the triangles.
        HX_FUNCTION() void FillMesh(engine::Mesh* mesh) const;
        HX_FUNCTION() void AddVert(Vector3 position, Color32 color, Vector4 uv0, Vector4 uv1, Vector4 uv2, Vector4 uv3, Vector3 normal, Vector4 tangent);
        HX_FUNCTION() void AddVert(Vector3 position, Color32 color, Vector4 uv0, Vector4 uv1, Vector3 normal, Vector4 tangent);
        HX_FUNCTION() void AddVert(Vector3 position, Color32 color, Vector4 uv0);
        HX_FUNCTION() void AddVert(engine::UIVertex v) { vertices_.Push(v); }
        HX_FUNCTION() void AddTriangle(i32 idx0, i32 idx1, i32 idx2);
        // Four vertices and their two triangles.
        HX_FUNCTION() void AddUIVertexQuad(Span<const engine::UIVertex> verts);
        HX_FUNCTION(Internal) void AddUIVertexStreamInternal(Span<const engine::UIVertex> verts, Span<const i32> indices);
        // A triangle list: each three vertices a triangle.
        HX_FUNCTION(Internal) void AddUIVertexTriangleStreamInternal(Span<const engine::UIVertex> verts);
        // Every triangle's vertices in order.
        HX_FUNCTION(Internal) Array<engine::UIVertex> GetUIVertexStreamInternal() const;

        Span<const engine::UIVertex> vertices() const { return Span<const engine::UIVertex>(vertices_.Data(), vertices_.Size()); }
        Span<const i32> indices() const { return Span<const i32>(indices_.Data(), indices_.Size()); }
        Array<engine::UIVertex>& MutableVertices() { return vertices_; }
        Array<i32>& MutableIndices() { return indices_; }

        scripting::TypeId HelixType() const override { return scripting::TypeOf<VertexHelper>(); }

    private:
        Array<engine::UIVertex> vertices_;
        Array<i32> indices_;
    };
}

HX_CANVASUI_ROOT_HANDLES(helix::canvasui::VertexHelper)
