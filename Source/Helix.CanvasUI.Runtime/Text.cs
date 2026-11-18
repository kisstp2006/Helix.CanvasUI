using System;
using System.Collections.Generic;

namespace HelixEngine.Text
{
    /// <summary>A character of the text's last generation (C#'s copy of the native one; its objects by id).</summary>
    public struct CharacterInfo
    {
        public char character;
        public int index;
        public int stringLength;
        public TextElementType elementType;
        public FontAsset fontAsset;
        public Material material;
        public int materialReferenceIndex;
        public bool isUsingAlternateTypeface;
        public float pointSize;
        public float scale;
        public float aspectRatio;
        public int lineNumber;
        public int pageNumber;
        public int vertexIndex;
        public Vertex vertex_BL;
        public Vertex vertex_TL;
        public Vertex vertex_TR;
        public Vertex vertex_BR;
        public Vector3 topLeft;
        public Vector3 bottomLeft;
        public Vector3 topRight;
        public Vector3 bottomRight;
        public float origin;
        public float xAdvance;
        public float ascender;
        public float baseLine;
        public float descender;
        public bool isVisible;
        public Color32 color;
        public Color32 underlineColor;
        public Color32 strikethroughColor;
        public Color32 highlightColor;
        public int underlineVertexIndex;
        public int strikethroughVertexIndex;
        public FontStyles style;

        internal static CharacterInfo From(CharacterInfoData d) => new CharacterInfo
        {
            character = (char)d.character,
            index = d.index,
            stringLength = d.stringLength,
            elementType = d.elementType,
            fontAsset = Resources.EntityIdToObject(d.fontAsset) as FontAsset,
            material = Resources.EntityIdToObject(d.material) as Material,
            materialReferenceIndex = d.materialReferenceIndex,
            isUsingAlternateTypeface = d.isUsingAlternateTypeface,
            pointSize = d.pointSize,
            scale = d.scale,
            aspectRatio = d.aspectRatio,
            lineNumber = d.lineNumber,
            pageNumber = d.pageNumber,
            vertexIndex = d.vertexIndex,
            vertex_BL = d.vertex_BL,
            vertex_TL = d.vertex_TL,
            vertex_TR = d.vertex_TR,
            vertex_BR = d.vertex_BR,
            topLeft = d.topLeft,
            bottomLeft = d.bottomLeft,
            topRight = d.topRight,
            bottomRight = d.bottomRight,
            origin = d.origin,
            xAdvance = d.xAdvance,
            ascender = d.ascender,
            baseLine = d.baseLine,
            descender = d.descender,
            isVisible = d.isVisible,
            color = d.color,
            underlineColor = d.underlineColor,
            strikethroughColor = d.strikethroughColor,
            highlightColor = d.highlightColor,
            underlineVertexIndex = d.underlineVertexIndex,
            strikethroughVertexIndex = d.strikethroughVertexIndex,
            style = d.style,
        };

        internal CharacterInfoData ToData() => new CharacterInfoData
        {
            character = character,
            index = index,
            stringLength = stringLength,
            elementType = elementType,
            fontAsset = fontAsset != null ? fontAsset.GetEntityId() : default,
            material = material != null ? material.GetEntityId() : default,
            materialReferenceIndex = materialReferenceIndex,
            isUsingAlternateTypeface = isUsingAlternateTypeface,
            pointSize = pointSize,
            scale = scale,
            aspectRatio = aspectRatio,
            lineNumber = lineNumber,
            pageNumber = pageNumber,
            vertexIndex = vertexIndex,
            vertex_BL = vertex_BL,
            vertex_TL = vertex_TL,
            vertex_TR = vertex_TR,
            vertex_BR = vertex_BR,
            topLeft = topLeft,
            bottomLeft = bottomLeft,
            topRight = topRight,
            bottomRight = bottomRight,
            origin = origin,
            xAdvance = xAdvance,
            ascender = ascender,
            baseLine = baseLine,
            descender = descender,
            isVisible = isVisible,
            color = color,
            underlineColor = underlineColor,
            strikethroughColor = strikethroughColor,
            highlightColor = highlightColor,
            underlineVertexIndex = underlineVertexIndex,
            strikethroughVertexIndex = strikethroughVertexIndex,
            style = style,
        };
    }

    public partial struct Vertex
    {
        public static Vertex zero { get; } = new Vertex();
    }

    public partial struct VertexGradient
    {
        public VertexGradient(Color color)
        {
            topLeft = topRight = bottomLeft = bottomRight = color;
        }

        public VertexGradient(Color color0, Color color1, Color color2, Color color3)
        {
            topLeft = color0;
            topRight = color1;
            bottomLeft = color2;
            bottomRight = color3;
        }
    }

    public struct WordInfo
    {
        public TextBase textComponent;
        public int firstCharacterIndex;
        public int lastCharacterIndex;
        public int characterCount;

        public string GetWord() => TextInfo.CharactersOf(textComponent, firstCharacterIndex, characterCount);
    }

    public struct LinkInfo
    {
        public TextBase textComponent;
        public int hashCode;
        public int linkIdFirstCharacterIndex;
        public int linkIdLength;
        public int linkTextfirstCharacterIndex;
        public int linkTextLength;
        internal string m_LinkId;

        public string GetLink() => m_LinkId;
        public string GetLinkID() => m_LinkId;
        public string GetLinkText() => TextInfo.CharactersOf(textComponent, linkTextfirstCharacterIndex, linkTextLength);
    }

    /// <summary>A mesh of the text with its vertex arrays (changed by scripts and sent back by UpdateVertexData).</summary>
    public struct MeshInfo
    {
        public Mesh mesh;
        public Material material;
        public int vertexCount;
        public Vector3[] vertices;
        public Vector3[] normals;
        public Vector4[] tangents;
        public Vector4[] uvs0;
        public Vector2[] uvs2;
        public Color32[] colors32;
        public int[] triangles;

        public MeshInfo(Mesh mesh, int size)
        {
            this.mesh = mesh;
            material = null;
            vertexCount = 0;
            vertices = normals = null;
            tangents = uvs0 = null;
            uvs2 = null;
            colors32 = null;
            triangles = null;
            ResizeMeshInfo(size);
        }

        public MeshInfo(Mesh mesh, int size, bool isVolumetric) : this(mesh, size)
        {
        }

        public void ResizeMeshInfo(int size)
        {
            int v = System.Math.Max(0, size) * 4;
            Array.Resize(ref vertices, v);
            Array.Resize(ref normals, v);
            Array.Resize(ref tangents, v);
            Array.Resize(ref uvs0, v);
            Array.Resize(ref uvs2, v);
            Array.Resize(ref colors32, v);
            Array.Resize(ref triangles, System.Math.Max(0, size) * 6);
            for (int q = 0; q < size; q++)
            {
                int b = q * 4, t = q * 6;
                triangles[t] = b;
                triangles[t + 1] = b + 1;
                triangles[t + 2] = b + 2;
                triangles[t + 3] = b + 2;
                triangles[t + 4] = b + 3;
                triangles[t + 5] = b;
            }
        }

        public void Clear() => Clear(true);

        public void Clear(bool uploadChanges)
        {
            if (vertices != null)
                Array.Clear(vertices, 0, vertices.Length);
            vertexCount = 0;
            if (uploadChanges && mesh != null)
                mesh.vertices = vertices;
        }

        public void ClearUnusedVertices()
        {
            if (vertices != null && vertexCount < vertices.Length)
                Array.Clear(vertices, vertexCount, vertices.Length - vertexCount);
        }

        public void ClearUnusedVertices(int startIndex)
        {
            if (vertices != null && startIndex < vertices.Length)
                Array.Clear(vertices, startIndex, vertices.Length - startIndex);
        }

        public void SortGeometry(VertexSortingOrder order)
        {
            if (order != VertexSortingOrder.Reverse)
                return;
            int quads = vertexCount / 4;
            for (int i = 0; i < quads / 2; i++)
                SwapVertexData(i * 4, (quads - 1 - i) * 4);
        }

        public void SwapVertexData(int src, int dst)
        {
            for (int k = 0; k < 4; k++)
            {
                (vertices[src + k], vertices[dst + k]) = (vertices[dst + k], vertices[src + k]);
                (uvs0[src + k], uvs0[dst + k]) = (uvs0[dst + k], uvs0[src + k]);
                (uvs2[src + k], uvs2[dst + k]) = (uvs2[dst + k], uvs2[src + k]);
                (colors32[src + k], colors32[dst + k]) = (colors32[dst + k], colors32[src + k]);
            }
        }
    }

    public partial class TextInfo
    {
        public CharacterInfo[] characterInfo = Array.Empty<CharacterInfo>();
        public WordInfo[] wordInfo = Array.Empty<WordInfo>();
        public LineInfo[] lineInfo = Array.Empty<LineInfo>();
        public LinkInfo[] linkInfo = Array.Empty<LinkInfo>();
        public PageInfo[] pageInfo = Array.Empty<PageInfo>();
        public MeshInfo[] meshInfo = Array.Empty<MeshInfo>();
        public TextBase textComponent;
        ulong m_Generation = ulong.MaxValue;

        public TextInfo(TextBase textComponent) : this()
        {
            this.textComponent = textComponent;
        }

        // The arrays from the native generation (again after each one).
        internal void Sync(TextBase owner)
        {
            textComponent = owner;
            ulong g = GenerationInternal();
            if (g == m_Generation)
                return;
            m_Generation = g;
            CharacterInfoData[] chars = CharacterInfoInternal();
            characterInfo = new CharacterInfo[chars.Length];
            for (int i = 0; i < chars.Length; i++)
                characterInfo[i] = CharacterInfo.From(chars[i]);
            WordInfoData[] words = WordInfoInternal();
            wordInfo = new WordInfo[words.Length];
            for (int i = 0; i < words.Length; i++)
                wordInfo[i] = new WordInfo
                {
                    textComponent = owner,
                    firstCharacterIndex = words[i].firstCharacterIndex,
                    lastCharacterIndex = words[i].lastCharacterIndex,
                    characterCount = words[i].characterCount,
                };
            lineInfo = LineInfoInternal();
            pageInfo = PageInfoInternal();
            LinkInfoData[] links = LinkInfoInternal();
            string[] ids = LinkIdsInternal();
            linkInfo = new LinkInfo[links.Length];
            for (int i = 0; i < links.Length; i++)
                linkInfo[i] = new LinkInfo
                {
                    textComponent = owner,
                    hashCode = links[i].hashCode,
                    linkIdFirstCharacterIndex = links[i].linkIdFirstCharacterIndex,
                    linkIdLength = links[i].linkIdLength,
                    linkTextfirstCharacterIndex = links[i].linkTextfirstCharacterIndex,
                    linkTextLength = links[i].linkTextLength,
                    m_LinkId = i < ids.Length ? ids[i] : "",
                };
            int meshes = MeshCountInternal();
            meshInfo = new MeshInfo[meshes];
            for (int i = 0; i < meshes; i++)
                meshInfo[i] = new MeshInfo
                {
                    mesh = MeshInternal(i),
                    material = MeshMaterialInternal(i),
                    vertexCount = MeshVertexCountInternal(i),
                    vertices = MeshVerticesInternal(i),
                    uvs0 = MeshUvs0Internal(i),
                    uvs2 = MeshUvs2Internal(i),
                    colors32 = MeshColorsInternal(i),
                    triangles = MeshTrianglesInternal(i),
                };
        }

        // The arrays a script changed, back to the native text info.
        internal void Push()
        {
            var chars = new CharacterInfoData[characterInfo.Length];
            for (int i = 0; i < chars.Length; i++)
                chars[i] = characterInfo[i].ToData();
            SetCharacterInfoInternal(chars);
            for (int i = 0; i < meshInfo.Length; i++)
                SetMeshDataInternal(i, meshInfo[i].vertices ?? Array.Empty<Vector3>(), meshInfo[i].uvs0 ?? Array.Empty<Vector4>(),
                                    meshInfo[i].uvs2 ?? Array.Empty<Vector2>(), meshInfo[i].colors32 ?? Array.Empty<Color32>());
        }

        public void ClearUnusedVertices(MaterialReference[] materials)
        {
            for (int i = 0; i < meshInfo.Length; i++)
                meshInfo[i].ClearUnusedVertices();
        }

        public MeshInfo[] CopyMeshInfoVertexData()
        {
            var copy = new MeshInfo[meshInfo.Length];
            for (int i = 0; i < meshInfo.Length; i++)
            {
                copy[i] = meshInfo[i];
                copy[i].vertices = (Vector3[])meshInfo[i].vertices?.Clone();
                copy[i].uvs0 = (Vector4[])meshInfo[i].uvs0?.Clone();
                copy[i].uvs2 = (Vector2[])meshInfo[i].uvs2?.Clone();
                copy[i].colors32 = (Color32[])meshInfo[i].colors32?.Clone();
            }
            return copy;
        }

        public static void Resize<T>(ref T[] array, int size)
        {
            int n = size > 1024 ? size + 256 : Mathf.NextPowerOfTwo(size);
            Array.Resize(ref array, n);
        }

        public static void Resize<T>(ref T[] array, int size, bool isBlockAllocated)
        {
            if (isBlockAllocated)
                size = size > 1024 ? size + 256 : Mathf.NextPowerOfTwo(size);
            if (array != null && size == array.Length)
                return;
            Array.Resize(ref array, size);
        }

        internal static string CharactersOf(TextBase text, int first, int count)
        {
            if (text == null)
                return "";
            CharacterInfo[] chars = text.textInfo.characterInfo;
            var s = new System.Text.StringBuilder();
            for (int i = first; i < first + count && i >= 0 && i < chars.Length; i++)
                s.Append(chars[i].character);
            return s.ToString();
        }
    }

    /// <summary>A material reference of the text's meshes (by index in the text's materials).</summary>
    public struct MaterialReference
    {
        public int index;
        public FontAsset fontAsset;
        public Material material;
        public bool isDefaultMaterial;
        public bool isFallbackMaterial;
        public Material fallbackMaterial;
        public float padding;
        public int referenceCount;

        public MaterialReference(int index, FontAsset fontAsset, Material material, float padding)
        {
            this.index = index;
            this.fontAsset = fontAsset;
            this.material = material;
            isDefaultMaterial = false;
            isFallbackMaterial = false;
            fallbackMaterial = null;
            this.padding = padding;
            referenceCount = 0;
        }
    }

    public abstract partial class TextBase
    {
        /// <summary>The info of the last generation (made now when a property changed).</summary>
        public TextInfo textInfo
        {
            get
            {
                TextInfo info = textInfoInternal;
                info.Sync(this);
                return info;
            }
        }

        Action<TextInfo> m_OnPreRenderText;
        bool m_PreRenderHooked;

        /// <summary>After the layout, before the meshes go up: scripts change the text info's vertices here.</summary>
        public virtual event Action<TextInfo> OnPreRenderText
        {
            add
            {
                m_OnPreRenderText += value;
                if (!m_PreRenderHooked)
                {
                    m_PreRenderHooked = true;
                    onPreRenderTextNative += info =>
                    {
                        info.Sync(this);
                        m_OnPreRenderText?.Invoke(info);
                        info.Push();
                    };
                }
            }
            remove => m_OnPreRenderText -= value;
        }

        public virtual TextInfo GetTextInfo(string text)
        {
            TextInfo info = GetTextInfoInternal(text);
            info.Sync(this);
            return info;
        }

        /// <summary>The text info's mesh arrays (changed by a script) to the meshes.</summary>
        public virtual void UpdateVertexData() => UpdateVertexData(VertexDataUpdateFlags.All);

        public virtual void UpdateVertexData(VertexDataUpdateFlags flags)
        {
            TextInfo info = textInfoInternal;
            info.Push();
            UpdateVertexDataInternal(flags);
        }

        public void SetText(System.Text.StringBuilder sourceText) => text = sourceText?.ToString() ?? "";

        public delegate void MissingCharacterEventCallback(int unicode, int stringIndex, string text, FontAsset fontAsset, TextBase textComponent);

        public struct CharacterSubstitution
        {
            public int index;
            public uint unicode;

            public CharacterSubstitution(int index, uint unicode)
            {
                this.index = index;
                this.unicode = unicode;
            }
        }
    }
}
