#pragma once

#include <Helix/CanvasUI/Core/OwnedRoot.h>
#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Object/EntityId.h>
#include <Helix/Math/Color.h>
#include <Helix/Math/Vector.h>
#include <Helix/UI/Engine/FontAsset.h>
#include <Helix/UI/Engine/UIAssets.h>

// The text components' shared types: their options (alignment, wrapping, overflow, styles, mappings) and the text
// info of a generation (characters, words, lines, links, pages and the meshes with their vertices). The C++ structs
// hold the objects as pointers; C# has its structs over the data structs, which hold them by id.
namespace helix::canvasui
{
    class TextBase;

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class HorizontalAlignmentOptions : i32
    {
        Left = 0x1,
        Center = 0x2,
        Right = 0x4,
        Justified = 0x8,
        Flush = 0x10,
        Geometry = 0x20,
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class VerticalAlignmentOptions : i32
    {
        Top = 0x100,
        Middle = 0x200,
        Bottom = 0x400,
        Baseline = 0x800,
        Geometry = 0x1000,
        Capline = 0x2000,
    };

    // Both axes in one value (a horizontal flag or'ed with a vertical one).
    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class TextAlignmentOptions : i32
    {
        TopLeft = 0x101,
        Top = 0x102,
        TopRight = 0x104,
        TopJustified = 0x108,
        TopFlush = 0x110,
        TopGeoAligned = 0x120,
        Left = 0x201,
        Center = 0x202,
        Right = 0x204,
        Justified = 0x208,
        Flush = 0x210,
        CenterGeoAligned = 0x220,
        BottomLeft = 0x401,
        Bottom = 0x402,
        BottomRight = 0x404,
        BottomJustified = 0x408,
        BottomFlush = 0x410,
        BottomGeoAligned = 0x420,
        BaselineLeft = 0x801,
        Baseline = 0x802,
        BaselineRight = 0x804,
        BaselineJustified = 0x808,
        BaselineFlush = 0x810,
        BaselineGeoAligned = 0x820,
        MidlineLeft = 0x1001,
        Midline = 0x1002,
        MidlineRight = 0x1004,
        MidlineJustified = 0x1008,
        MidlineFlush = 0x1010,
        MidlineGeoAligned = 0x1020,
        CaplineLeft = 0x2001,
        Capline = 0x2002,
        CaplineRight = 0x2004,
        CaplineJustified = 0x2008,
        CaplineFlush = 0x2010,
        CaplineGeoAligned = 0x2020,
        Converted = 0xFFFF,
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class TextWrappingModes : i32
    {
        NoWrap = 0,
        Normal = 1,
        PreserveWhitespace = 2,
        PreserveWhitespaceNoWrap = 3,
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class TextOverflowModes : i32
    {
        Overflow = 0,
        Ellipsis = 1,
        Masking = 2,
        Truncate = 3,
        ScrollRect = 4,
        Page = 5,
        Linked = 6,
    };

    HX_ENUM(Namespace = "HelixEngine.Text", Attributes = "[System.Flags]")
    enum class FontStyles : i32
    {
        Normal = 0x0,
        Bold = 0x1,
        Italic = 0x2,
        Underline = 0x4,
        LowerCase = 0x8,
        UpperCase = 0x10,
        SmallCaps = 0x20,
        Strikethrough = 0x40,
        Superscript = 0x80,
        Subscript = 0x100,
        Highlight = 0x200,
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class FontWeight : i32
    {
        Thin = 100,
        ExtraLight = 200,
        Light = 300,
        Regular = 400,
        Medium = 500,
        SemiBold = 600,
        Bold = 700,
        Heavy = 800,
        Black = 900,
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class TextureMappingOptions : i32
    {
        Character = 0,
        Line = 1,
        Paragraph = 2,
        MatchAspect = 3,
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class VertexSortingOrder : i32
    {
        Normal = 0,
        Reverse = 1,
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class TextRenderFlags : i32
    {
        DontRender = 0x0,
        Render = 0xFF,
    };

    // The kinds of text elements are the font asset's (characters and sprites).
    using TextElementType = ui::TextElementType;

    HX_ENUM(Namespace = "HelixEngine.Text", Attributes = "[System.Flags]")
    enum class VertexDataUpdateFlags : i32
    {
        None = 0x0,
        Vertices = 0x1,
        Uv0 = 0x2,
        Uv2 = 0x4,
        Uv4 = 0x8,
        Colors32 = 0x10,
        All = 0xFF,
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class MaskingTypes : i32
    {
        MaskOff = 0,
        MaskHard = 1,
        MaskSoft = 2,
    };

    HX_ENUM(Namespace = "HelixEngine.Text")
    enum class ColorMode : i32
    {
        Single = 0,
        HorizontalGradient = 1,
        VerticalGradient = 2,
        FourCornersGradient = 3,
    };

    HX_STRUCT(Namespace = "HelixEngine.Text")
    struct VertexGradient
    {
        HX_FIELD() Color topLeft = Color(1, 1, 1, 1);
        HX_FIELD() Color topRight = Color(1, 1, 1, 1);
        HX_FIELD() Color bottomLeft = Color(1, 1, 1, 1);
        HX_FIELD() Color bottomRight = Color(1, 1, 1, 1);

        VertexGradient() = default;
        explicit VertexGradient(Color c) : topLeft(c), topRight(c), bottomLeft(c), bottomRight(c) {}
        VertexGradient(Color tl, Color tr, Color bl, Color br) : topLeft(tl), topRight(tr), bottomLeft(bl), bottomRight(br) {}
    };

    HX_STRUCT(Namespace = "HelixEngine.Text")
    struct Extents
    {
        HX_FIELD() Vector2 min = Vector2(0, 0);
        HX_FIELD() Vector2 max = Vector2(0, 0);
    };

    HX_STRUCT(Namespace = "HelixEngine.Text")
    struct Vertex
    {
        HX_FIELD() Vector3 position = Vector3(0, 0, 0);
        HX_FIELD() Vector4 uv = Vector4(0, 0, 0, 0);
        HX_FIELD() Vector2 uv2 = Vector2(0, 0);
        HX_FIELD() Color32 color = Color32(0, 0, 0, 0);
    };

    // A character of the last generation: where it is, its quad's corners (the text's local space, y up), its font,
    // style and color, its line and page, and where its vertices are in its mesh.
    struct CharacterInfo
    {
        char32_t character = 0;
        i32 index = 0, stringLength = 0;
        TextElementType elementType = TextElementType::Character;
        ui::FontAsset* fontAsset = nullptr;
        engine::Material* material = nullptr;
        i32 materialReferenceIndex = 0;
        bool isUsingAlternateTypeface = false;
        f32 pointSize = 0, scale = 1, aspectRatio = 1;
        i32 lineNumber = 0, pageNumber = 0, vertexIndex = 0;
        Vertex vertex_BL, vertex_TL, vertex_TR, vertex_BR;
        Vector3 topLeft = Vector3(0, 0, 0), bottomLeft = Vector3(0, 0, 0), topRight = Vector3(0, 0, 0), bottomRight = Vector3(0, 0, 0);
        f32 origin = 0, xAdvance = 0, ascender = 0, baseLine = 0, descender = 0;
        bool isVisible = false;
        Color32 color = Color32(255, 255, 255, 255), underlineColor = Color32(255, 255, 255, 255), strikethroughColor = Color32(255, 255, 255, 255),
                highlightColor = Color32(255, 255, 0, 64);
        i32 underlineVertexIndex = 0, strikethroughVertexIndex = 0;
        FontStyles style = FontStyles::Normal;
    };

    // CharacterInfo across the bindings (the objects by id).
    HX_STRUCT(Namespace = "HelixEngine.Text", Internal)
    struct CharacterInfoData
    {
        HX_FIELD() u32 character = 0;
        HX_FIELD() i32 index = 0;
        HX_FIELD() i32 stringLength = 0;
        HX_FIELD() TextElementType elementType = TextElementType::Character;
        HX_FIELD() engine::EntityId fontAsset;
        HX_FIELD() engine::EntityId material;
        HX_FIELD() i32 materialReferenceIndex = 0;
        HX_FIELD() bool isUsingAlternateTypeface = false;
        HX_FIELD() f32 pointSize = 0;
        HX_FIELD() f32 scale = 1;
        HX_FIELD() f32 aspectRatio = 1;
        HX_FIELD() i32 lineNumber = 0;
        HX_FIELD() i32 pageNumber = 0;
        HX_FIELD() i32 vertexIndex = 0;
        HX_FIELD() Vertex vertex_BL;
        HX_FIELD() Vertex vertex_TL;
        HX_FIELD() Vertex vertex_TR;
        HX_FIELD() Vertex vertex_BR;
        HX_FIELD() Vector3 topLeft = Vector3(0, 0, 0);
        HX_FIELD() Vector3 bottomLeft = Vector3(0, 0, 0);
        HX_FIELD() Vector3 topRight = Vector3(0, 0, 0);
        HX_FIELD() Vector3 bottomRight = Vector3(0, 0, 0);
        HX_FIELD() f32 origin = 0;
        HX_FIELD() f32 xAdvance = 0;
        HX_FIELD() f32 ascender = 0;
        HX_FIELD() f32 baseLine = 0;
        HX_FIELD() f32 descender = 0;
        HX_FIELD() bool isVisible = false;
        HX_FIELD() Color32 color = Color32(255, 255, 255, 255);
        HX_FIELD() Color32 underlineColor = Color32(255, 255, 255, 255);
        HX_FIELD() Color32 strikethroughColor = Color32(255, 255, 255, 255);
        HX_FIELD() Color32 highlightColor = Color32(255, 255, 0, 64);
        HX_FIELD() i32 underlineVertexIndex = 0;
        HX_FIELD() i32 strikethroughVertexIndex = 0;
        HX_FIELD() FontStyles style = FontStyles::Normal;

        static CharacterInfoData From(const CharacterInfo& c);
    };

    HX_STRUCT(Namespace = "HelixEngine.Text")
    struct LineInfo
    {
        HX_FIELD() i32 characterCount = 0;
        HX_FIELD() i32 visibleCharacterCount = 0;
        HX_FIELD() i32 spaceCount = 0;
        HX_FIELD() i32 visibleSpaceCount = 0;
        HX_FIELD() i32 wordCount = 0;
        HX_FIELD() i32 firstCharacterIndex = 0;
        HX_FIELD() i32 firstVisibleCharacterIndex = 0;
        HX_FIELD() i32 lastCharacterIndex = 0;
        HX_FIELD() i32 lastVisibleCharacterIndex = 0;
        HX_FIELD() f32 length = 0;
        HX_FIELD() f32 width = 0;
        HX_FIELD() f32 lineHeight = 0;
        HX_FIELD() f32 ascender = 0;
        HX_FIELD() f32 baseline = 0;
        HX_FIELD() f32 descender = 0;
        HX_FIELD() f32 maxAdvance = 0;
        HX_FIELD() f32 marginLeft = 0;
        HX_FIELD() f32 marginRight = 0;
        HX_FIELD() HorizontalAlignmentOptions alignment = HorizontalAlignmentOptions::Left;
        HX_FIELD() Extents lineExtents;
    };

    // Words, links and pages by their characters (C# adds the component and the text they name).
    HX_STRUCT(Namespace = "HelixEngine.Text", Internal)
    struct WordInfoData
    {
        HX_FIELD() i32 firstCharacterIndex = 0;
        HX_FIELD() i32 lastCharacterIndex = 0;
        HX_FIELD() i32 characterCount = 0;
    };

    struct WordInfo
    {
        TextBase* textComponent = nullptr;
        i32 firstCharacterIndex = 0, lastCharacterIndex = 0, characterCount = 0;
        // The word's characters.
        String GetWord() const;
    };

    HX_STRUCT(Namespace = "HelixEngine.Text", Internal)
    struct LinkInfoData
    {
        HX_FIELD() i32 hashCode = 0;
        HX_FIELD() i32 linkIdFirstCharacterIndex = 0;
        HX_FIELD() i32 linkIdLength = 0;
        HX_FIELD() i32 linkTextfirstCharacterIndex = 0;
        HX_FIELD() i32 linkTextLength = 0;
    };

    struct LinkInfo
    {
        TextBase* textComponent = nullptr;
        i32 hashCode = 0, linkIdFirstCharacterIndex = 0, linkIdLength = 0, linkTextfirstCharacterIndex = 0, linkTextLength = 0;
        String linkId;
        String GetLink() const { return GetLinkID(); }
        String GetLinkID() const { return linkId; }
        String GetLinkText() const;
    };

    HX_STRUCT(Namespace = "HelixEngine.Text")
    struct PageInfo
    {
        HX_FIELD() i32 firstCharacterIndex = 0;
        HX_FIELD() i32 lastCharacterIndex = 0;
        HX_FIELD() f32 ascender = 0;
        HX_FIELD() f32 baseLine = 0;
        HX_FIELD() f32 descender = 0;
    };

    // A mesh of the text: its material's quads (the main font's first atlas page is mesh 0; other pages, fallback
    // fonts and sprites have their own).
    struct MeshInfo
    {
        engine::Mesh* mesh = nullptr;
        engine::Material* material = nullptr;
        i32 vertexCount = 0;
        Array<Vector3> vertices, normals;
        Array<Vector4> tangents, uvs0;
        Array<Vector2> uvs2;
        Array<Color32> colors32;
        Array<i32> triangles;

        void ResizeMeshInfo(i32 size);
        // The vertices zeroed (uploaded to the mesh when asked).
        void Clear(bool uploadChanges = true);
        void ClearUnusedVertices();
        // The quads in the order (reversed: the first character drawn last).
        void SortGeometry(VertexSortingOrder order);
        // A quad's four vertices swapped with another's.
        void SwapVertexData(i32 src, i32 dst);
    };

    // The info of a text's last generation.
    HX_CLASS(Namespace = "HelixEngine.Text", Root, Owned)
    class TextInfo : public OwnedRoot<TextInfo>
    {
    public:
        HX_FUNCTION(Constructor) TextInfo() = default;
        explicit TextInfo(TextBase* component) : textComponent(component) {}

        HX_FIELD() i32 characterCount = 0;
        HX_FIELD() i32 spaceCount = 0;
        HX_FIELD() i32 spriteCount = 0;
        HX_FIELD() i32 wordCount = 0;
        HX_FIELD() i32 linkCount = 0;
        HX_FIELD() i32 lineCount = 0;
        HX_FIELD() i32 pageCount = 0;
        HX_FIELD() i32 materialCount = 0;

        Array<CharacterInfo> characterInfo;
        Array<WordInfo> wordInfo;
        Array<LineInfo> lineInfo;
        Array<LinkInfo> linkInfo;
        Array<PageInfo> pageInfo;
        Array<MeshInfo> meshInfo;
        TextBase* textComponent = nullptr;

        HX_FUNCTION() void ClearAllMeshInfo();
        HX_FUNCTION() void ClearMeshInfo(bool updateMesh);
        HX_FUNCTION() void ResetVertexLayout(bool isVolumetric) { (void)isVolumetric; }
        // A copy of the meshes' vertex data (for effects that change the vertices from the generated ones).
        Array<MeshInfo> CopyMeshInfoVertexData() const;
        void Clear();

        // For C#: the arrays as data.
        HX_FUNCTION(Internal) Array<CharacterInfoData> CharacterInfoInternal() const;
        HX_FUNCTION(Internal) void SetCharacterInfoInternal(Span<const CharacterInfoData> value);
        HX_FUNCTION(Internal) Array<WordInfoData> WordInfoInternal() const;
        HX_FUNCTION(Internal) Array<LineInfo> LineInfoInternal() const { return lineInfo; }
        HX_FUNCTION(Internal) Array<LinkInfoData> LinkInfoInternal() const;
        HX_FUNCTION(Internal) Array<String> LinkIdsInternal() const;
        HX_FUNCTION(Internal) Array<PageInfo> PageInfoInternal() const { return pageInfo; }
        HX_FUNCTION(Internal) i32 MeshCountInternal() const { return i32(meshInfo.Size()); }
        HX_FUNCTION(Internal) engine::Mesh* MeshInternal(i32 index) const;
        HX_FUNCTION(Internal) engine::Material* MeshMaterialInternal(i32 index) const;
        HX_FUNCTION(Internal) i32 MeshVertexCountInternal(i32 index) const;
        HX_FUNCTION(Internal) Array<Vector3> MeshVerticesInternal(i32 index) const;
        HX_FUNCTION(Internal) Array<Vector4> MeshUvs0Internal(i32 index) const;
        HX_FUNCTION(Internal) Array<Vector2> MeshUvs2Internal(i32 index) const;
        HX_FUNCTION(Internal) Array<Color32> MeshColorsInternal(i32 index) const;
        HX_FUNCTION(Internal) Array<i32> MeshTrianglesInternal(i32 index) const;
        HX_FUNCTION(Internal) void SetMeshDataInternal(i32 index, Span<const Vector3> vertices, Span<const Vector4> uvs0, Span<const Vector2> uvs2,
                                                       Span<const Color32> colors);
        HX_FUNCTION(Internal) u64 GenerationInternal() const { return generation; }

        // Counts the generations (C# reads the arrays again after one).
        u64 generation = 0;

        scripting::TypeId HelixType() const override { return scripting::TypeOf<TextInfo>(); }
    };
}

HX_CANVASUI_ROOT_HANDLES(helix::canvasui::TextInfo)
