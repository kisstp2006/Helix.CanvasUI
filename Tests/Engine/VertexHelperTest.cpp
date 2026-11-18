#include <Helix/CanvasUI/Rendering/VertexHelper.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::canvasui;

HX_TEST(VertexHelper, QuadsStreamsAndMeshes)
{
    VertexHelper vh;
    vh.AddVert(Vector3(0, 0, 0), Color32(255, 0, 0, 255), Vector4(0, 0, 0, 0));
    vh.AddVert(Vector3(0, 1, 0), Color32(0, 255, 0, 255), Vector4(0, 1, 0, 0));
    vh.AddVert(Vector3(1, 1, 0), Color32(0, 0, 255, 255), Vector4(1, 1, 0, 0));
    vh.AddVert(Vector3(1, 0, 0), Color32(255, 255, 255, 255), Vector4(1, 0, 0, 0));
    vh.AddTriangle(0, 1, 2);
    vh.AddTriangle(2, 3, 0);
    HX_CHECK(vh.currentVertCount() == 4 && vh.currentIndexCount() == 6);
    engine::UIVertex v;
    vh.PopulateUIVertex(v, 2);
    HX_CHECK(v.position == Vector3(1, 1, 0) && v.normal == Vector3(0, 0, -1) && v.tangent == Vector4(1, 0, 0, -1));
    v.color = Color32(9, 9, 9, 9);
    vh.SetUIVertex(v, 2);
    Array<engine::UIVertex> stream = vh.GetUIVertexStreamInternal();
    HX_CHECK(stream.Size() == 6 && stream[2].color == Color32(9, 9, 9, 9) && stream[5].position == Vector3(0, 0, 0));
    // Into a mesh and back.
    engine::Mesh* mesh = engine::Mesh::Create();
    vh.FillMesh(mesh);
    VertexHelper back(mesh);
    HX_CHECK(back.currentVertCount() == 4 && back.currentIndexCount() == 6 && back.vertices()[2].color == Color32(9, 9, 9, 9));
    // A triangle list keeps its shared vertices once.
    VertexHelper tri;
    tri.AddUIVertexTriangleStreamInternal(Span<const engine::UIVertex>(stream.Data(), stream.Size()));
    HX_CHECK(tri.currentVertCount() == 4 && tri.currentIndexCount() == 6);
    engine::UIVertex quad[4] = {back.vertices()[0], back.vertices()[1], back.vertices()[2], back.vertices()[3]};
    tri.AddUIVertexQuad(Span<const engine::UIVertex>(quad, 4));
    HX_CHECK(tri.currentVertCount() == 8 && tri.indices()[11] == 4);
    tri.Clear();
    HX_CHECK(tri.currentVertCount() == 0 && tri.currentIndexCount() == 0);
    engine::Object::DestroyImmediate(mesh);
}
