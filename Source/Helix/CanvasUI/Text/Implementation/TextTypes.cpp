#include <Helix/CanvasUI/Text/TextBase.h>

namespace helix::canvasui
{
    CharacterInfoData CharacterInfoData::From(const CharacterInfo& c)
    {
        CharacterInfoData d;
        d.character = u32(c.character);
        d.index = c.index;
        d.stringLength = c.stringLength;
        d.elementType = c.elementType;
        d.fontAsset = c.fontAsset ? c.fontAsset->GetEntityId() : engine::EntityId();
        d.material = c.material ? c.material->GetEntityId() : engine::EntityId();
        d.materialReferenceIndex = c.materialReferenceIndex;
        d.isUsingAlternateTypeface = c.isUsingAlternateTypeface;
        d.pointSize = c.pointSize;
        d.scale = c.scale;
        d.aspectRatio = c.aspectRatio;
        d.lineNumber = c.lineNumber;
        d.pageNumber = c.pageNumber;
        d.vertexIndex = c.vertexIndex;
        d.vertex_BL = c.vertex_BL;
        d.vertex_TL = c.vertex_TL;
        d.vertex_TR = c.vertex_TR;
        d.vertex_BR = c.vertex_BR;
        d.topLeft = c.topLeft;
        d.bottomLeft = c.bottomLeft;
        d.topRight = c.topRight;
        d.bottomRight = c.bottomRight;
        d.origin = c.origin;
        d.xAdvance = c.xAdvance;
        d.ascender = c.ascender;
        d.baseLine = c.baseLine;
        d.descender = c.descender;
        d.isVisible = c.isVisible;
        d.color = c.color;
        d.underlineColor = c.underlineColor;
        d.strikethroughColor = c.strikethroughColor;
        d.highlightColor = c.highlightColor;
        d.underlineVertexIndex = c.underlineVertexIndex;
        d.strikethroughVertexIndex = c.strikethroughVertexIndex;
        d.style = c.style;
        return d;
    }

    namespace
    {
        // A text's characters from first to last (as UTF-8).
        String CharactersOf(const TextBase* text, i32 first, i32 count)
        {
            String out;
            if (!text || !const_cast<TextBase*>(text)->textInfo())
                return out;
            const TextInfo* info = const_cast<TextBase*>(text)->textInfo();
            for (i32 i = first; i < first + count && i >= 0 && usize(i) < info->characterInfo.Size(); i++)
            {
                u32 c = u32(info->characterInfo[usize(i)].character);
                if (c < 0x80)
                    out += char(c);
                else if (c < 0x800)
                    out += char(0xC0 | (c >> 6)), out += char(0x80 | (c & 0x3F));
                else if (c < 0x10000)
                    out += char(0xE0 | (c >> 12)), out += char(0x80 | ((c >> 6) & 0x3F)), out += char(0x80 | (c & 0x3F));
                else
                    out += char(0xF0 | (c >> 18)), out += char(0x80 | ((c >> 12) & 0x3F)), out += char(0x80 | ((c >> 6) & 0x3F)), out += char(0x80 | (c & 0x3F));
            }
            return out;
        }
    }

    String WordInfo::GetWord() const { return CharactersOf(textComponent, firstCharacterIndex, characterCount); }

    String LinkInfo::GetLinkText() const { return CharactersOf(textComponent, linkTextfirstCharacterIndex, linkTextLength); }

    // ------------------------------------------------------------------------------------------------- MeshInfo

    void MeshInfo::ResizeMeshInfo(i32 size)
    {
        usize v = usize(size < 0 ? 0 : size) * 4;
        vertices.Resize(v);
        normals.Resize(v);
        tangents.Resize(v);
        uvs0.Resize(v);
        uvs2.Resize(v);
        colors32.Resize(v);
        triangles.Resize(usize(size < 0 ? 0 : size) * 6);
        for (i32 q = 0; q < size; q++)
        {
            i32 b = q * 4;
            i32 t = q * 6;
            triangles[usize(t)] = b, triangles[usize(t + 1)] = b + 1, triangles[usize(t + 2)] = b + 2;
            triangles[usize(t + 3)] = b + 2, triangles[usize(t + 4)] = b + 3, triangles[usize(t + 5)] = b;
        }
    }

    void MeshInfo::Clear(bool uploadChanges)
    {
        for (Vector3& p : vertices)
            p = Vector3(0, 0, 0);
        vertexCount = 0;
        if (uploadChanges && mesh)
            mesh->SetVertices(Span<const Vector3>(vertices.Data(), vertices.Size()));
    }

    void MeshInfo::ClearUnusedVertices()
    {
        for (usize i = usize(vertexCount); i < vertices.Size(); i++)
            vertices[i] = Vector3(0, 0, 0);
    }

    void MeshInfo::SwapVertexData(i32 src, i32 dst)
    {
        i32 a = src * 4, b = dst * 4;
        if (a < 0 || b < 0 || usize(a + 3) >= vertices.Size() || usize(b + 3) >= vertices.Size())
            return;
        for (i32 k = 0; k < 4; k++)
        {
            std::swap(vertices[usize(a + k)], vertices[usize(b + k)]);
            std::swap(uvs0[usize(a + k)], uvs0[usize(b + k)]);
            std::swap(uvs2[usize(a + k)], uvs2[usize(b + k)]);
            std::swap(colors32[usize(a + k)], colors32[usize(b + k)]);
        }
    }

    void MeshInfo::SortGeometry(VertexSortingOrder order)
    {
        if (order != VertexSortingOrder::Reverse)
            return;
        i32 quads = vertexCount / 4;
        for (i32 i = 0; i < quads / 2; i++)
            SwapVertexData(i, quads - 1 - i);
    }

    // ------------------------------------------------------------------------------------------------- TextInfo

    void TextInfo::Clear()
    {
        characterCount = spaceCount = spriteCount = wordCount = linkCount = lineCount = pageCount = 0;
        characterInfo.Clear();
        wordInfo.Clear();
        lineInfo.Clear();
        linkInfo.Clear();
        pageInfo.Clear();
    }

    void TextInfo::ClearAllMeshInfo()
    {
        for (MeshInfo& m : meshInfo)
            m.Clear(true);
    }

    void TextInfo::ClearMeshInfo(bool updateMesh)
    {
        for (MeshInfo& m : meshInfo)
            m.Clear(updateMesh);
    }

    Array<MeshInfo> TextInfo::CopyMeshInfoVertexData() const
    {
        Array<MeshInfo> out;
        for (const MeshInfo& m : meshInfo)
        {
            MeshInfo c;
            c.vertexCount = m.vertexCount;
            c.vertices = m.vertices;
            c.uvs0 = m.uvs0;
            c.uvs2 = m.uvs2;
            c.colors32 = m.colors32;
            out.Push(std::move(c));
        }
        return out;
    }

    Array<CharacterInfoData> TextInfo::CharacterInfoInternal() const
    {
        Array<CharacterInfoData> out;
        for (const CharacterInfo& c : characterInfo)
            out.Push(CharacterInfoData::From(c));
        return out;
    }

    void TextInfo::SetCharacterInfoInternal(Span<const CharacterInfoData> value)
    {
        // A script's changes of what it may change (colors, visibility, the vertex records).
        for (usize i = 0; i < value.Size() && i < characterInfo.Size(); i++)
        {
            CharacterInfo& c = characterInfo[i];
            const CharacterInfoData& d = value[i];
            c.isVisible = d.isVisible;
            c.color = d.color;
            c.vertex_BL = d.vertex_BL, c.vertex_TL = d.vertex_TL, c.vertex_TR = d.vertex_TR, c.vertex_BR = d.vertex_BR;
            c.underlineColor = d.underlineColor, c.strikethroughColor = d.strikethroughColor, c.highlightColor = d.highlightColor;
        }
    }

    Array<WordInfoData> TextInfo::WordInfoInternal() const
    {
        Array<WordInfoData> out;
        for (const WordInfo& w : wordInfo)
            out.Push(WordInfoData{w.firstCharacterIndex, w.lastCharacterIndex, w.characterCount});
        return out;
    }

    Array<LinkInfoData> TextInfo::LinkInfoInternal() const
    {
        Array<LinkInfoData> out;
        for (const LinkInfo& l : linkInfo)
            out.Push(LinkInfoData{l.hashCode, l.linkIdFirstCharacterIndex, l.linkIdLength, l.linkTextfirstCharacterIndex, l.linkTextLength});
        return out;
    }

    Array<String> TextInfo::LinkIdsInternal() const
    {
        Array<String> out;
        for (const LinkInfo& l : linkInfo)
            out.Push(l.linkId);
        return out;
    }

    namespace
    {
        const MeshInfo* At(const Array<MeshInfo>& m, i32 i) { return i >= 0 && usize(i) < m.Size() ? &m[usize(i)] : nullptr; }
    }

    engine::Mesh* TextInfo::MeshInternal(i32 index) const { return At(meshInfo, index) ? At(meshInfo, index)->mesh : nullptr; }
    engine::Material* TextInfo::MeshMaterialInternal(i32 index) const { return At(meshInfo, index) ? At(meshInfo, index)->material : nullptr; }
    i32 TextInfo::MeshVertexCountInternal(i32 index) const { return At(meshInfo, index) ? At(meshInfo, index)->vertexCount : 0; }
    Array<Vector3> TextInfo::MeshVerticesInternal(i32 index) const { return At(meshInfo, index) ? At(meshInfo, index)->vertices : Array<Vector3>(); }
    Array<Vector4> TextInfo::MeshUvs0Internal(i32 index) const { return At(meshInfo, index) ? At(meshInfo, index)->uvs0 : Array<Vector4>(); }
    Array<Vector2> TextInfo::MeshUvs2Internal(i32 index) const { return At(meshInfo, index) ? At(meshInfo, index)->uvs2 : Array<Vector2>(); }
    Array<Color32> TextInfo::MeshColorsInternal(i32 index) const { return At(meshInfo, index) ? At(meshInfo, index)->colors32 : Array<Color32>(); }
    Array<i32> TextInfo::MeshTrianglesInternal(i32 index) const { return At(meshInfo, index) ? At(meshInfo, index)->triangles : Array<i32>(); }

    void TextInfo::SetMeshDataInternal(i32 index, Span<const Vector3> vertices, Span<const Vector4> uvs0, Span<const Vector2> uvs2, Span<const Color32> colors)
    {
        if (index < 0 || usize(index) >= meshInfo.Size())
            return;
        MeshInfo& m = meshInfo[usize(index)];
        auto copy = [](auto& dst, auto src) {
            for (usize i = 0; i < src.Size() && i < dst.Size(); i++)
                dst[i] = src[i];
        };
        copy(m.vertices, vertices);
        copy(m.uvs0, uvs0);
        copy(m.uvs2, uvs2);
        copy(m.colors32, colors);
    }
}
