#include <Helix/CanvasUI/Effects/MeshEffects.h>

namespace helix::canvasui
{
    Graphic* BaseMeshEffect::graphic() const { return ComponentOf<Graphic>(gameObject()); }

    void BaseMeshEffect::Dirty()
    {
        if (Graphic* g = graphic())
            g->SetVerticesDirty();
    }

    void BaseMeshEffect::ModifyMeshOf(engine::Mesh* mesh)
    {
        VertexHelper vh(mesh);
        ModifyMesh(&vh);
        vh.FillMesh(mesh);
    }

    void BaseMeshEffect::OnEnable()
    {
        UIBehaviour::OnEnable();
        Dirty();
    }

    void BaseMeshEffect::OnDisable()
    {
        Dirty();
        UIBehaviour::OnDisable();
    }

    void BaseMeshEffect::OnDidApplyAnimationProperties() { Dirty(); }

    void Shadow::SetEffectColor(Color value)
    {
        effectColor_ = value;
        Dirty();
    }

    void Shadow::SetEffectDistance(Vector2 value)
    {
        constexpr f32 kMax = 600;
        value.x = value.x > kMax ? kMax : (value.x < -kMax ? -kMax : value.x);
        value.y = value.y > kMax ? kMax : (value.y < -kMax ? -kMax : value.y);
        if (effectDistance_ == value)
            return;
        effectDistance_ = value;
        Dirty();
    }

    void Shadow::SetUseGraphicAlpha(bool value)
    {
        useGraphicAlpha_ = value;
        Dirty();
    }

    void Shadow::ApplyShadowTo(Array<engine::UIVertex>& verts, Color32 color, i32 start, i32 end, f32 x, f32 y) const
    {
        for (i32 i = start; i < end; i++)
        {
            engine::UIVertex v = verts[usize(i)];
            verts.Push(v);
            v.position.x += x;
            v.position.y += y;
            Color32 c = color;
            if (useGraphicAlpha_)
                c.a = u8((u32(c.a) * u32(verts[usize(i)].color.a)) / 255u);
            v.color = c;
            verts[usize(i)] = v;
        }
    }

    void Shadow::ModifyMesh(VertexHelper* vh)
    {
        if (!IsActive())
            return;
        Array<engine::UIVertex> verts = vh->GetUIVertexStreamInternal();
        ApplyShadowTo(verts, effectColor_, 0, i32(verts.Size()), effectDistance_.x, effectDistance_.y);
        vh->Clear();
        vh->AddUIVertexTriangleStreamInternal(Span<const engine::UIVertex>(verts.Data(), verts.Size()));
    }

    void Outline::ModifyMesh(VertexHelper* vh)
    {
        if (!IsActive())
            return;
        Array<engine::UIVertex> verts = vh->GetUIVertexStreamInternal();
        Vector2 d = effectDistance();
        Color32 c = effectColor();
        const Vector2 offsets[4] = {Vector2(d.x, d.y), Vector2(d.x, -d.y), Vector2(-d.x, d.y), Vector2(-d.x, -d.y)};
        i32 start = 0;
        for (const Vector2& o : offsets)
        {
            i32 end = i32(verts.Size());
            ApplyShadowTo(verts, c, start, end, o.x, o.y);
            start = end;
        }
        vh->Clear();
        vh->AddUIVertexTriangleStreamInternal(Span<const engine::UIVertex>(verts.Data(), verts.Size()));
    }

    void PositionAsUV1::ModifyMesh(VertexHelper* vh)
    {
        engine::UIVertex v;
        for (i32 i = 0; i < vh->currentVertCount(); i++)
        {
            vh->PopulateUIVertex(v, i);
            v.uv1 = Vector4(v.position.x, v.position.y, 0, 0);
            vh->SetUIVertex(v, i);
        }
    }
}
