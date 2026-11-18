#include <Helix/CanvasUI/Rendering/VertexHelper.h>

#include <Helix/Engine/Mesh/Mesh.h>

namespace helix::canvasui
{
    VertexHelper::VertexHelper(engine::Mesh* m)
    {
        if (!m)
            return;
        Array<Vector3> positions, normals;
        Array<Color32> colors;
        Array<Vector4> uv[4], tangents;
        m->GetVertices(positions);
        m->GetColors(colors);
        for (i32 c = 0; c < 4; c++)
            m->GetUVs(c, uv[c]);
        m->GetNormals(normals);
        m->GetTangents(tangents);
        for (usize i = 0; i < positions.Size(); i++)
        {
            engine::UIVertex v;
            v.position = positions[i];
            v.color = i < colors.Size() ? colors[i] : Color32(255, 255, 255, 255);
            Vector4* channels[4] = {&v.uv0, &v.uv1, &v.uv2, &v.uv3};
            for (i32 c = 0; c < 4; c++)
                *channels[c] = i < uv[c].Size() ? uv[c][i] : Vector4(0, 0, 0, 0);
            v.normal = i < normals.Size() ? normals[i] : Vector3(0, 0, 0);
            v.tangent = i < tangents.Size() ? tangents[i] : Vector4(0, 0, 0, 0);
            vertices_.Push(v);
        }
        Array<i32> triangles;
        for (i32 s = 0; s < m->subMeshCount(); s++)
        {
            m->GetTriangles(triangles, s);
            for (i32 t : triangles)
                indices_.Push(t);
        }
    }

    void VertexHelper::PopulateUIVertex(engine::UIVertex& vertex, i32 i) const
    {
        if (i >= 0 && usize(i) < vertices_.Size())
            vertex = vertices_[usize(i)];
    }

    void VertexHelper::SetUIVertex(engine::UIVertex vertex, i32 i)
    {
        if (i >= 0 && usize(i) < vertices_.Size())
            vertices_[usize(i)] = vertex;
    }

    void VertexHelper::FillMesh(engine::Mesh* mesh) const
    {
        if (!mesh)
            return;
        mesh->Clear();
        // More than 65000 vertices would not fit 16 bit indices (the reference's limit).
        u32 n = u32(vertices_.Size());
        Array<Vector3> positions, normals;
        Array<Color32> colors;
        Array<Vector4> uv[4], tangents;
        for (const engine::UIVertex& v : vertices_)
        {
            positions.Push(v.position);
            colors.Push(v.color);
            uv[0].Push(v.uv0), uv[1].Push(v.uv1), uv[2].Push(v.uv2), uv[3].Push(v.uv3);
            normals.Push(v.normal);
            tangents.Push(v.tangent);
        }
        mesh->SetVertices(Span<const Vector3>(positions.Data(), n));
        mesh->SetColors(Span<const Color32>(colors.Data(), n));
        for (i32 c = 0; c < 4; c++)
            mesh->SetUVs(c, Span<const Vector4>(uv[c].Data(), n));
        mesh->SetNormals(Span<const Vector3>(normals.Data(), n));
        mesh->SetTangents(Span<const Vector4>(tangents.Data(), n));
        mesh->SetTriangles(Span<const i32>(indices_.Data(), indices_.Size()));
        mesh->RecalculateBounds();
    }

    void VertexHelper::AddVert(Vector3 position, Color32 color, Vector4 uv0, Vector4 uv1, Vector4 uv2, Vector4 uv3, Vector3 normal, Vector4 tangent)
    {
        engine::UIVertex v;
        v.position = position, v.color = color, v.uv0 = uv0, v.uv1 = uv1, v.uv2 = uv2, v.uv3 = uv3, v.normal = normal, v.tangent = tangent;
        vertices_.Push(v);
    }

    void VertexHelper::AddVert(Vector3 position, Color32 color, Vector4 uv0, Vector4 uv1, Vector3 normal, Vector4 tangent)
    {
        AddVert(position, color, uv0, uv1, Vector4(0, 0, 0, 0), Vector4(0, 0, 0, 0), normal, tangent);
    }

    // The reference's defaults: a normal facing back, a tangent along x.
    void VertexHelper::AddVert(Vector3 position, Color32 color, Vector4 uv0)
    {
        AddVert(position, color, uv0, Vector4(0, 0, 0, 0), Vector3(0, 0, -1), Vector4(1, 0, 0, -1));
    }

    void VertexHelper::AddTriangle(i32 idx0, i32 idx1, i32 idx2)
    {
        indices_.Push(idx0);
        indices_.Push(idx1);
        indices_.Push(idx2);
    }

    void VertexHelper::AddUIVertexQuad(Span<const engine::UIVertex> verts)
    {
        i32 start = currentVertCount();
        for (usize i = 0; i < 4 && i < verts.Size(); i++)
            vertices_.Push(verts[i]);
        AddTriangle(start, start + 1, start + 2);
        AddTriangle(start + 2, start + 3, start);
    }

    void VertexHelper::AddUIVertexStreamInternal(Span<const engine::UIVertex> verts, Span<const i32> indices)
    {
        for (const engine::UIVertex& v : verts)
            vertices_.Push(v);
        for (i32 i : indices)
            indices_.Push(i);
    }

    void VertexHelper::AddUIVertexTriangleStreamInternal(Span<const engine::UIVertex> verts)
    {
        Array<Vector3> positions, normals;
        Array<Color32> colors;
        Array<Vector4> uv0, uv1, uv2, uv3, tangents;
        Array<i32> indices;
        engine::CanvasRenderer::SplitUIVertexStreams(verts, positions, colors, uv0, uv1, uv2, uv3, normals, tangents, indices);
        i32 base = currentVertCount();
        for (usize i = 0; i < positions.Size(); i++)
            AddVert(positions[i], colors[i], uv0[i], uv1[i], uv2[i], uv3[i], normals[i], tangents[i]);
        for (i32 i : indices)
            indices_.Push(base + i);
    }

    Array<engine::UIVertex> VertexHelper::GetUIVertexStreamInternal() const
    {
        Array<engine::UIVertex> out;
        for (i32 i : indices_)
            if (i >= 0 && usize(i) < vertices_.Size())
                out.Push(vertices_[usize(i)]);
        return out;
    }
}
