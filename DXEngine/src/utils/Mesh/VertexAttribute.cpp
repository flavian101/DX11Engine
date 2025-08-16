#include "dxpch.h"
#include "VertexAttribute.h"
#include <cassert>
#include <sstream>
#include <algorithm>
#include "MeshResource.h"

namespace DXEngine
{
	uint32_t VertexAttribute::GetSize() const
	{
        switch (Format)
        {
        case DataFormat::Float:     return sizeof(float);
        case DataFormat::Float2:    return sizeof(float) * 2;
        case DataFormat::Float3:    return sizeof(float) * 3;
        case DataFormat::Float4:    return sizeof(float) * 4;
        case DataFormat::Int:       return sizeof(int32_t);
        case DataFormat::Int2:      return sizeof(int32_t) * 2;
        case DataFormat::Int3:      return sizeof(int32_t) * 3;
        case DataFormat::Int4:      return sizeof(int32_t) * 4;
        case DataFormat::UByte4:    return sizeof(uint8_t) * 4;
        case DataFormat::UByte4N:   return sizeof(uint8_t) * 4;
        case DataFormat::Short2:    return sizeof(int16_t) * 2;
        case DataFormat::Short2N:   return sizeof(int16_t) * 2;
        case DataFormat::Short4:    return sizeof(int16_t) * 4;
        case DataFormat::Short4N:   return sizeof(int16_t) * 4;
        case DataFormat::Half2:     return sizeof(uint16_t) * 2; // Half precision
        case DataFormat::Half4:     return sizeof(uint16_t) * 4; // Half precision
        default:                    return 0;
        }
    }

    DXGI_FORMAT VertexAttribute::GetDXGIFormat() const
    {
        switch (Format)
        {
        case DataFormat::Float:     return DXGI_FORMAT_R32_FLOAT;
        case DataFormat::Float2:    return DXGI_FORMAT_R32G32_FLOAT;
        case DataFormat::Float3:    return DXGI_FORMAT_R32G32B32_FLOAT;
        case DataFormat::Float4:    return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case DataFormat::Int:       return DXGI_FORMAT_R32_SINT;
        case DataFormat::Int2:      return DXGI_FORMAT_R32G32_SINT;
        case DataFormat::Int3:      return DXGI_FORMAT_R32G32B32_SINT;
        case DataFormat::Int4:      return DXGI_FORMAT_R32G32B32A32_SINT;
        case DataFormat::UByte4:    return DXGI_FORMAT_R8G8B8A8_UINT;
        case DataFormat::UByte4N:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DataFormat::Short2:    return DXGI_FORMAT_R16G16_SINT;
        case DataFormat::Short2N:   return DXGI_FORMAT_R16G16_SNORM;
        case DataFormat::Short4:    return DXGI_FORMAT_R16G16B16A16_SINT;
        case DataFormat::Short4N:   return DXGI_FORMAT_R16G16B16A16_SNORM;
        case DataFormat::Half2:     return DXGI_FORMAT_R16G16_FLOAT;
        case DataFormat::Half4:     return DXGI_FORMAT_R16G16B16A16_FLOAT;
        default:                    return DXGI_FORMAT_UNKNOWN;
        }
    }

    std::string VertexAttribute::GetDefaultSemanticName(VertexAttributeType type)
    {
        switch (type)
        {
        case VertexAttributeType::Position:     return "POSITION";
        case VertexAttributeType::Normal:       return "NORMAL";
        case VertexAttributeType::Tangent:      return "TANGENT";
        case VertexAttributeType::Bitangent:    return "BITANGENT";
        case VertexAttributeType::TexCoord0:    return "TEXCOORD";
        case VertexAttributeType::TexCoord1:    return "TEXCOORD";
        case VertexAttributeType::TexCoord2:    return "TEXCOORD";
        case VertexAttributeType::TexCoord3:    return "TEXCOORD";
        case VertexAttributeType::Color0:       return "COLOR";
        case VertexAttributeType::Color1:       return "COLOR";
        case VertexAttributeType::BlendIndices: return "BLENDINDICES";
        case VertexAttributeType::BlendWeights: return "BLENDWEIGHT";
        case VertexAttributeType::Custom:       return "CUSTOM";
        default:                                return "UNKNOWN";
        }
    }

    //vertexLayout
    VertexLayout& VertexLayout::Add(const VertexAttribute& attribute)
    {
        assert(!m_Finalized && "cannot add attributes to finalize layout");
        m_Attributes.push_back(attribute);
        return *this;
    }

    VertexLayout& VertexLayout::Add(VertexAttributeType type, DataFormat format, uint32_t slot, bool perInstance)
    {
        return Add(VertexAttribute(type, format, "", 0, slot, perInstance));
    }

    VertexLayout& VertexLayout::Position(DataFormat format, uint32_t slot)
    {
        return Add(VertexAttributeType::Position, format, slot);
    }

    VertexLayout& VertexLayout::Normal(DataFormat format, uint32_t slot)
    {
        return Add(VertexAttributeType::Normal, format, slot);
    }

    VertexLayout& VertexLayout::Tangent(DataFormat format, uint32_t slot)
    {
        return Add(VertexAttributeType::Tangent, format, slot);
    }

    VertexLayout& VertexLayout::TexCoord(uint32_t index, DataFormat format, uint32_t slot)
    {
        VertexAttributeType type = static_cast<VertexAttributeType>(static_cast<int>(VertexAttributeType::TexCoord0) + index);
        VertexAttribute attr(type, format, "TEXCOORD", index, slot);
        return Add(attr);
    }

    VertexLayout& VertexLayout::Color(uint32_t index, DataFormat format, uint32_t slot)
    {
        VertexAttributeType type = static_cast<VertexAttributeType>(static_cast<int>(VertexAttributeType::Color0) + index);
        VertexAttribute attr(type, format, "COLOR", index, slot);
        return Add(attr);
    }

    VertexLayout& VertexLayout::BlendData(DataFormat indicesFormat, DataFormat weightsFormat, uint32_t slot)
    {
        Add(VertexAttributeType::BlendIndices, indicesFormat, slot);
        Add(VertexAttributeType::BlendWeights, weightsFormat, slot);
        return *this;
    }

    void VertexLayout::Finalize()
    {
        if (m_Finalized)
            return;

        CalculateOffsetsAndStrides();
        m_Finalized = true;
    }

    uint32_t VertexLayout::GetStride(uint32_t slot) const
    {
        auto it = m_SlotStrides.find(slot);
        return it != m_SlotStrides.end() ? it->second : 0;
    }

    std::vector<D3D11_INPUT_ELEMENT_DESC> VertexLayout::CreateD3D11InputElements() const
    {
        assert(m_Finalized && "Layout must be finalized before creating D3D11 elements");

        std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
        elements.reserve(m_Attributes.size());

        for (const auto& attr : m_Attributes)
        {
            D3D11_INPUT_ELEMENT_DESC desc = {};
            desc.SemanticName = attr.SemanticName.c_str();
            desc.SemanticIndex = attr.SemanticIndex;
            desc.Format = attr.GetDXGIFormat();
            desc.InputSlot = attr.Slot;
            desc.AlignedByteOffset = attr.Offset;
            desc.InputSlotClass = attr.PerInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
            desc.InstanceDataStepRate = attr.PerInstance ? 1 : 0;

            elements.push_back(desc);
        }

        return elements;
    }

    bool VertexLayout::HasAttribute(VertexAttributeType type, uint32_t slot) const
    {
        return FindAttribute(type, slot) != nullptr;
    }

    const VertexAttribute* VertexLayout::FindAttribute(VertexAttributeType type, uint32_t slot) const
    {
        auto it = std::find_if(m_Attributes.begin(), m_Attributes.end(),
            [type, slot](const VertexAttribute& attr)
            {
                return attr.Type == type && attr.Slot == slot;
            });
        return it != m_Attributes.end() ? &(*it) : nullptr;

    }

    std::string VertexLayout::GetDebugString() const
    {
        std::ostringstream oss{};
        oss << "VertexLayout (" << m_Attributes.size() << " attributes):\n";

        for (size_t i = 0; i < m_Attributes.size(); ++i)
        {
            const auto& attr = m_Attributes[i];
            oss << "  [" << i << "] " << attr.SemanticName << attr.SemanticIndex
                << " (slot:" << attr.Slot << ", offset:" << attr.Offset
                << ", size:" << attr.GetSize() << ")\n";
        }

        oss << "Slot strides: ";
        for (const auto& [slot, stride] : m_SlotStrides)
        {
            oss << "slot" << slot << ":" << stride << "b ";
        }

        return oss.str();
    }

    void VertexLayout::CalculateOffsetsAndStrides()
    {
        m_SlotStrides.clear();

        std::unordered_map<uint32_t, std::vector<VertexAttribute*>> slotAttributes;
        for (auto& attr : m_Attributes)
        {
            slotAttributes[attr.Slot].push_back(&attr);
        }
        //calculate
        for (auto& [slot, attributes] : slotAttributes)
        {
            uint32_t currentOffset = 0;

            for (auto* attr : attributes)
            {
                attr->Offset = currentOffset;
                currentOffset += attr->GetSize();
            }

            m_SlotStrides[slot] = currentOffset;
        }

    }
    //basic layouts
    VertexLayout VertexLayout::CreateBasic()
    {
        VertexLayout layout;
        layout.Position()
              .Normal()
              .TexCoord(0)
              .Finalize();
        return layout;
    }

    VertexLayout VertexLayout::CreateLit()
    {
        VertexLayout layout;
        layout.Position()
              .Normal()
              .Tangent()
              .TexCoord(0)
              .Finalize();
        return layout;
    }

    VertexLayout VertexLayout::CreateUI()
    {
        VertexLayout layout;
        layout.Position(DataFormat::Float2)
              .TexCoord(0)
              .Color(0)
              .Finalize();
        return layout;
    }

    VertexLayout VertexLayout::CreateSkinned()
    {
        VertexLayout layout;
        layout.Position()
              .Normal()
              .Tangent()
              .TexCoord(0)
              .BlendData()
              .Finalize();
        return layout;
    }

    VertexLayout VertexLayout::CreateParticle()
    {
        VertexLayout layout;
        layout.Position()
              .Color(0)
              .Add(VertexAttributeType::TexCoord0, DataFormat::Float2) // Size
              .Finalize();
        return layout;
    }


    //VertexData implementation
    VertexData::VertexData(const VertexLayout& layout) : m_Layout(layout)
    {
        assert(layout.IsFinalized() && "Layout must be finalized");

        // Initialize data containers for each slot
        for (const auto& attr : layout.GetAttributes())
        {
            if (m_Data.find(attr.Slot) == m_Data.end())
            {
                m_Data[attr.Slot] = std::vector<uint8_t>();
            }
        }
    }

    void VertexData::Reserve(size_t vertexCount)
    {
        for (auto& [slot, data] : m_Data)
        {
            uint32_t stride = m_Layout.GetStride(slot);
            data.reserve(vertexCount * stride);
        }
    }

    void IndexData::SetIndexType(IndexType type)
    {
        if (m_IndexType == type)
            return;

        //convert existing indices to new type if needed
        if (GetIndexCount() > 0)
        {
            std::vector<uint32_t> tempIndices;
            tempIndices.reserve(GetIndexCount());

            //extract all indices as uint32_t
            for (size_t i = 0; i < GetIndexCount(); ++i)
            {
                tempIndices.push_back(GetIndex(i));
            }

            //cleare current data and set new type
            m_IndexType = type;
            Clear();

            //add back the indices in the new format
            if (type == IndexType::UInt16)
            {
                m_Indices = std::vector<uint16_t>();
                auto& indices16 = std::get<std::vector<uint16_t>>(m_Indices);
                indices16.reserve(tempIndices.size());

                for (uint32_t index : tempIndices)
                {
                    // Clamp to uint16_t range
                    indices16.push_back(static_cast<uint16_t>(std::min(index, static_cast<uint32_t>(UINT16_MAX))));
                }
            }
            else
            {
                m_Indices = std::vector<uint32_t>(std::move(tempIndices));
            }
        }
        else
        {
            m_IndexType = type;
                m_IndexType = type;
                Clear();
        }

    }
    

    void IndexData::Reserve(size_t count)
    {
        if (m_IndexType == IndexType::UInt16)
        {
            if (std::holds_alternative<std::vector<uint16_t>>(m_Indices))
            {
                std::get<std::vector<uint16_t>>(m_Indices).reserve(count);
            }
            else
            {
                m_Indices = std::vector<uint16_t>();
                std::get<std::vector<uint16_t>>(m_Indices).reserve(count);
            }
        }
        else
        {
            if (std::holds_alternative<std::vector<uint32_t>>(m_Indices))
            {
                std::get<std::vector<uint32_t>>(m_Indices).reserve(count);
            }
            else
            {
                m_Indices = std::vector<uint32_t>();
                std::get<std::vector<uint32_t>>(m_Indices).reserve(count);
            }
        }
    }

    void IndexData::Clear()
    {
        if (m_IndexType == IndexType::UInt16)
        {
            m_Indices = std::vector<uint16_t>();
        }
        else
        {
            m_Indices = std::vector<uint32_t>();
        }
    }

    void IndexData::AddIndex(uint32_t index)
    {
        if (m_IndexType == IndexType::UInt16)
        {
            // Ensure we have the correct variant type
            if (!std::holds_alternative<std::vector<uint16_t>>(m_Indices))
            {
                m_Indices = std::vector<uint16_t>();
            }

            // Check if index fits in uint16_t
            if (index > UINT16_MAX)
            {
                OutputDebugStringA("Warning: Index value exceeds uint16_t range, clamping to UINT16_MAX\n");
                index = UINT16_MAX;
            }

            std::get<std::vector<uint16_t>>(m_Indices).push_back(static_cast<uint16_t>(index));
        }
        else
        {
            // Ensure we have the correct variant type
            if (!std::holds_alternative<std::vector<uint32_t>>(m_Indices))
            {
                m_Indices = std::vector<uint32_t>();
            }

            std::get<std::vector<uint32_t>>(m_Indices).push_back(index);
        }
    }

    void IndexData::AddTriangle(uint32_t i0, uint32_t i1, uint32_t i2)
    {
        AddIndex(i0);
        AddIndex(i1);
        AddIndex(i2);
    }

    void IndexData::AddQuad(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3)
    {
        // Create two triangles: (i0, i1, i2) and (i2, i3, i0)
        AddTriangle(i0, i1, i2);
        AddTriangle(i2, i3, i0);
    }

    void IndexData::SetIndices(const std::vector<uint16_t>& indices)
    {
        m_IndexType = IndexType::UInt16;
        m_Indices = indices;
    }

    void IndexData::SetIndices(const std::vector<uint32_t>& indices)
    {
        m_IndexType = IndexType::UInt32;
        m_Indices = indices;
    }

    template<typename T>
    void IndexData::SetIndices(const std::vector<T>& indices)
    {
        if constexpr (std::is_same_v<T, uint16_t>)
        {
            SetIndices(indices);
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            SetIndices(indices);
        }
        else
        {
            // Convert to appropriate type
            if (m_IndexType == IndexType::UInt16)
            {
                std::vector<uint16_t> converted;
                converted.reserve(indices.size());
                for (const auto& index : indices)
                {
                    uint32_t val = static_cast<uint32_t>(index);
                    if (val > UINT16_MAX)
                    {
                        OutputDebugStringA("Warning: Index value exceeds uint16_t range, clamping\n");
                        val = UINT16_MAX;
                    }
                    converted.push_back(static_cast<uint16_t>(val));
                }
                SetIndices(converted);
            }
            else
            {
                std::vector<uint32_t> converted;
                converted.reserve(indices.size());
                for (const auto& index : indices)
                {
                    converted.push_back(static_cast<uint32_t>(index));
                }
                SetIndices(converted);
            }
        }
    }

    // Explicit template instantiations for common types
    template void IndexData::SetIndices(const std::vector<int>& indices);
    template void IndexData::SetIndices(const std::vector<size_t>& indices);

    size_t IndexData::GetIndexCount() const
    {
        if (std::holds_alternative<std::vector<uint16_t>>(m_Indices))
        {
            return std::get<std::vector<uint16_t>>(m_Indices).size();
        }
        else if (std::holds_alternative<std::vector<uint32_t>>(m_Indices))
        {
            return std::get<std::vector<uint32_t>>(m_Indices).size();
        }
        return 0;
    }

    size_t IndexData::GetDataSize() const
    {
        if (m_IndexType == IndexType::UInt16)
        {
            return GetIndexCount() * sizeof(uint16_t);
        }
        else
        {
            return GetIndexCount() * sizeof(uint32_t);
        }
    }

    const void* IndexData::GetData() const
    {
        if (std::holds_alternative<std::vector<uint16_t>>(m_Indices))
        {
            const auto& indices = std::get<std::vector<uint16_t>>(m_Indices);
            return indices.empty() ? nullptr : indices.data();
        }
        else if (std::holds_alternative<std::vector<uint32_t>>(m_Indices))
        {
            const auto& indices = std::get<std::vector<uint32_t>>(m_Indices);
            return indices.empty() ? nullptr : indices.data();
        }
        return nullptr;
    }

    void* IndexData::GetData()
    {
        if (std::holds_alternative<std::vector<uint16_t>>(m_Indices))
        {
            auto& indices = std::get<std::vector<uint16_t>>(m_Indices);
            return indices.empty() ? nullptr : indices.data();
        }
        else if (std::holds_alternative<std::vector<uint32_t>>(m_Indices))
        {
            auto& indices = std::get<std::vector<uint32_t>>(m_Indices);
            return indices.empty() ? nullptr : indices.data();
        }
        return nullptr;
    }

    uint32_t IndexData::GetIndex(size_t index) const
    {
        assert(index < GetIndexCount() && "Index out of bounds");

        if (std::holds_alternative<std::vector<uint16_t>>(m_Indices))
        {
            return static_cast<uint32_t>(std::get<std::vector<uint16_t>>(m_Indices)[index]);
        }
        else if (std::holds_alternative<std::vector<uint32_t>>(m_Indices))
        {
            return std::get<std::vector<uint32_t>>(m_Indices)[index];
        }
        return 0;
    }

    void IndexData::SetIndex(size_t index, uint32_t value)
    {
        assert(index < GetIndexCount() && "Index out of bounds");

        if (std::holds_alternative<std::vector<uint16_t>>(m_Indices))
        {
            if (value > UINT16_MAX)
            {
                OutputDebugStringA("Warning: Index value exceeds uint16_t range, clamping\n");
                value = UINT16_MAX;
            }
            std::get<std::vector<uint16_t>>(m_Indices)[index] = static_cast<uint16_t>(value);
        }
        else if (std::holds_alternative<std::vector<uint32_t>>(m_Indices))
        {
            std::get<std::vector<uint32_t>>(m_Indices)[index] = value;
        }
    }

    void IndexData::OptimizeForCache()
    {
        // Basic vertex cache optimization using a simplified version of Tom Forsyth's algorithm
        // This is a simplified implementation - for production use, consider using meshoptimizer library

        size_t indexCount = GetIndexCount();
        if (indexCount < 3 || indexCount % 3 != 0)
        {
            return; // Need complete triangles
        }

        size_t triangleCount = indexCount / 3;

        // Create a copy of the current indices
        std::vector<uint32_t> originalIndices;
        originalIndices.reserve(indexCount);
        for (size_t i = 0; i < indexCount; ++i)
        {
            originalIndices.push_back(GetIndex(i));
        }

        // Find the maximum vertex index to determine vertex count
        uint32_t maxVertex = 0;
        for (uint32_t idx : originalIndices)
        {
            maxVertex = std::max(maxVertex, idx);
        }
        size_t vertexCount = maxVertex + 1;

        // Build adjacency information
        std::vector<std::vector<size_t>> vertexTriangles(vertexCount);
        for (size_t tri = 0; tri < triangleCount; ++tri)
        {
            for (int v = 0; v < 3; ++v)
            {
                uint32_t vertex = originalIndices[tri * 3 + v];
                vertexTriangles[vertex].push_back(tri);
            }
        }

        // Simple greedy optimization
        std::vector<bool> processedTriangles(triangleCount, false);
        std::vector<uint32_t> optimizedIndices;
        optimizedIndices.reserve(indexCount);

        // Simple cache simulation (typical cache size is 16-32 vertices)
        const size_t CACHE_SIZE = 24;
        std::vector<uint32_t> cache;

        // Start with triangle 0
        size_t currentTriangle = 0;

        while (optimizedIndices.size() < indexCount)
        {
            // Find next best triangle if current is processed
            if (processedTriangles[currentTriangle])
            {
                size_t bestTriangle = triangleCount;
                int bestScore = -1;

                // Look for unprocessed triangles that share vertices with cache
                for (uint32_t cacheVertex : cache)
                {
                    for (size_t tri : vertexTriangles[cacheVertex])
                    {
                        if (!processedTriangles[tri])
                        {
                            // Count how many vertices of this triangle are in cache
                            int score = 0;
                            for (int v = 0; v < 3; ++v)
                            {
                                uint32_t vertex = originalIndices[tri * 3 + v];
                                auto it = std::find(cache.begin(), cache.end(), vertex);
                                if (it != cache.end())
                                {
                                    score += (CACHE_SIZE - std::distance(cache.begin(), it));
                                }
                            }

                            if (score > bestScore)
                            {
                                bestScore = score;
                                bestTriangle = tri;
                            }
                        }
                    }
                }

                // If no connected triangle found, pick any unprocessed triangle
                if (bestTriangle == triangleCount)
                {
                    for (size_t tri = 0; tri < triangleCount; ++tri)
                    {
                        if (!processedTriangles[tri])
                        {
                            bestTriangle = tri;
                            break;
                        }
                    }
                }

                currentTriangle = bestTriangle;
            }

            if (currentTriangle >= triangleCount)
                break;

            // Add triangle to optimized list
            processedTriangles[currentTriangle] = true;

            for (int v = 0; v < 3; ++v)
            {
                uint32_t vertex = originalIndices[currentTriangle * 3 + v];
                optimizedIndices.push_back(vertex);

                // Update cache (LRU)
                auto it = std::find(cache.begin(), cache.end(), vertex);
                if (it != cache.end())
                {
                    cache.erase(it);
                }
                cache.insert(cache.begin(), vertex);

                if (cache.size() > CACHE_SIZE)
                {
                    cache.pop_back();
                }
            }

            // Find next triangle (prefer one that shares vertices)
            currentTriangle = triangleCount;
        }

        // Update the indices with optimized order
        Clear();
        if (m_IndexType == IndexType::UInt16)
        {
            std::vector<uint16_t> indices16;
            indices16.reserve(optimizedIndices.size());
            for (uint32_t idx : optimizedIndices)
            {
                indices16.push_back(static_cast<uint16_t>(std::min(idx, static_cast<uint32_t>(UINT16_MAX))));
            }
            SetIndices(indices16);
        }
        else
        {
            SetIndices(optimizedIndices);
        }

        OutputDebugStringA("IndexData::OptimizeForCache - Vertex cache optimization completed\n");
    }

   // void IndexData::GenerateAdjacency(const VertexData& vertices, std::vector<uint32_t>& adjacency)
   // {
   //     // Silence unused param for now (indices-only adjacency).
   //     (void)vertices;
   //
   //     const size_t indexCount = GetIndexCount();
   //     if (indexCount % 3 != 0)
   //     {
   //         OutputDebugStringA("Warning: Index count is not a multiple of 3, cannot generate adjacency\n");
   //         adjacency.clear();
   //         return;
   //     }
   //
   //     const size_t triangleCount = indexCount / 3;
   //
   //     adjacency.clear();
   //     adjacency.reserve(triangleCount * 6); // 6 indices per triangle for adjacency topology
   //
   //     // Track degenerate triangles (any repeated vertex).
   //     std::vector<uint8_t> isDegenerate(triangleCount, 0);
   //
   //     struct Edge
   //     {
   //         uint32_t v0, v1;      // undirected key (sorted)
   //         size_t triangle;      // triangle index
   //         int edgeIndex;        // 0:(i0,i1), 1:(i1,i2), 2:(i2,i0)
   //
   //         Edge(uint32_t a, uint32_t b, size_t tri, int edge)
   //             : v0(std::min(a, b)), v1(std::max(a, b)), triangle(tri), edgeIndex(edge) {
   //         }
   //
   //         bool operator<(const Edge& other) const
   //         {
   //             if (v0 != other.v0) return v0 < other.v0;
   //             if (v1 != other.v1) return v1 < other.v1;
   //             // Tie-breakers (not required for correctness, but deterministic)
   //             if (triangle != other.triangle) return triangle < other.triangle;
   //             return edgeIndex < other.edgeIndex;
   //         }
   //
   //         bool matches(const Edge& other) const { return v0 == other.v0 && v1 == other.v1; }
   //     };
   //
   //     std::vector<Edge> edges;
   //     edges.reserve(triangleCount * 3);
   //
   //     // Collect all non-degenerate triangle edges
   //     for (size_t tri = 0; tri < triangleCount; ++tri)
   //     {
   //         const uint32_t i0 = GetIndex(tri * 3 + 0);
   //         const uint32_t i1 = GetIndex(tri * 3 + 1);
   //         const uint32_t i2 = GetIndex(tri * 3 + 2);
   //
   //         const bool deg = (i0 == i1) || (i1 == i2) || (i2 == i0);
   //         isDegenerate[tri] = static_cast<uint8_t>(deg);
   //
   //         if (!deg)
   //         {
   //             edges.emplace_back(i0, i1, tri, 0);
   //             edges.emplace_back(i1, i2, tri, 1);
   //             edges.emplace_back(i2, i0, tri, 2);
   //         }
   //     }
   //
   //     std::sort(edges.begin(), edges.end());
   //
   //     // For each triangle edge, store the neighboring triangle index or UINT32_MAX if none.
   //     std::vector<std::array<uint32_t, 3>> triangleAdjacency(
   //         triangleCount, { UINT32_MAX, UINT32_MAX, UINT32_MAX });
   //
   //     // Scan equal-edge runs (robust against non-manifold edges with >2 incident triangles).
   //     for (size_t i = 0; i < edges.size(); )
   //     {
   //         size_t j = i + 1;
   //         while (j < edges.size() && edges[j].matches(edges[i])) ++j;
   //
   //         const size_t runLen = j - i;
   //         if (runLen == 2)
   //         {
   //             const Edge& e0 = edges[i];
   //             const Edge& e1 = edges[i + 1];
   //
   //             triangleAdjacency[e0.triangle][e0.edgeIndex] = static_cast<uint32_t>(e1.triangle);
   //             triangleAdjacency[e1.triangle][e1.edgeIndex] = static_cast<uint32_t>(e0.triangle);
   //         }
   //         else if (runLen > 2)
   //         {
   //             // Non-manifold edge: pick first two deterministically, ignore the rest but warn.
   //             const Edge& e0 = edges[i];
   //             const Edge& e1 = edges[i + 1];
   //
   //             triangleAdjacency[e0.triangle][e0.edgeIndex] = static_cast<uint32_t>(e1.triangle);
   //             triangleAdjacency[e1.triangle][e1.edgeIndex] = static_cast<uint32_t>(e0.triangle);
   //
   //             OutputDebugStringA("Warning: Non-manifold edge detected; using first two triangles for adjacency\n");
   //         }
   //
   //         i = j;
   //     }
   //
   //     // Build adjacency index buffer: (v0, adj01, v1, adj12, v2, adj20) per triangle.
   //     // For boundary edges, use the triangle's own opposite vertex (safe, valid, and common practice).
   //     for (size_t tri = 0; tri < triangleCount; ++tri)
   //     {
   //         const uint32_t i0 = GetIndex(tri * 3 + 0);
   //         const uint32_t i1 = GetIndex(tri * 3 + 1);
   //         const uint32_t i2 = GetIndex(tri * 3 + 2);
   //
   //         // Edge (i0, i1)
   //         auto pushAdjOpposite = [&](size_t adjTri, uint32_t a, uint32_t b) -> uint32_t
   //             {
   //                 if (adjTri == UINT32_MAX)
   //                 {
   //                     // Boundary: use this triangle's opposite vertex
   //                     // for (i0,i1) -> i2, for (i1,i2) -> i0, for (i2,i0) -> i1
   //                     if (a == i0 && b == i1) return i2;
   //                     else if (a == i1 && b == i2) return i0;
   //                     else /*(a == i2 && b == i0)*/ return i1;
   //                 }
   //
   //                 const uint32_t ai0 = GetIndex(adjTri * 3 + 0);
   //                 const uint32_t ai1 = GetIndex(adjTri * 3 + 1);
   //                 const uint32_t ai2 = GetIndex(adjTri * 3 + 2);
   //
   //                 // Return the vertex in the adjacent triangle that is opposite to edge (a,b).
   //                 if (ai0 != a && ai0 != b) return ai0;
   //                 if (ai1 != a && ai1 != b) return ai1;
   //                 return ai2; // Safe fallback (handles rare degenerate neighbors)
   //             };
   //
   //         adjacency.push_back(i0);
   //         adjacency.push_back(pushAdjOpposite(triangleAdjacency[tri][0], i0, i1));
   //         adjacency.push_back(i1);
   //         adjacency.push_back(pushAdjOpposite(triangleAdjacency[tri][1], i1, i2));
   //         adjacency.push_back(i2);
   //         adjacency.push_back(pushAdjOpposite(triangleAdjacency[tri][2], i2, i0));
   //     }
   //
   //     char debugMsg[256];
   //     sprintf_s(debugMsg, "IndexData::GenerateAdjacency - Generated adjacency for %zu triangles\n", triangleCount);
   //     OutputDebugStringA(debugMsg);
   // }


    void VertexData::Resize(size_t vertexCount)
    {
        m_VertexCount = vertexCount;

        for (auto& [slot, data] : m_Data)
        {
            uint32_t stride = m_Layout.GetStride(slot);
            data.resize(vertexCount * stride);
        }
    }

    void VertexData::Clear()
    {
        for (auto& [slot, data] : m_Data)
        {
            data.clear();
        }
        m_VertexCount = 0;
    }

    bool VertexData::IsValid() const
    {
        if (m_VertexCount == 0)
            return false;

        for (const auto& [slot, data] : m_Data)
        {
            uint32_t expectedSize = m_VertexCount * m_Layout.GetStride(slot);
            if (data.size() != expectedSize)
                return false;
        }

        return true;
    }
    template<>
    void VertexData::SetAttribute<float>(size_t vertexIndex, VertexAttributeType type,
        const float& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(float));
    }

    template<>
    float VertexData::GetAttribute<float>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        float result;
        memcpy(&result, attrStart, sizeof(float));
        return result;
    }

    // Additional template specializations for integer types
    template<>
    void VertexData::SetAttribute<uint32_t>(size_t vertexIndex, VertexAttributeType type,
        const uint32_t& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(uint32_t));
    }

    template<>
    uint32_t VertexData::GetAttribute<uint32_t>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        uint32_t result;
        memcpy(&result, attrStart, sizeof(uint32_t));
        return result;
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMFLOAT3>(size_t vertexIndex, VertexAttributeType type,
        const DirectX::XMFLOAT3& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMFLOAT3));
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMFLOAT2>(size_t vertexIndex, VertexAttributeType type,
        const DirectX::XMFLOAT2& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMFLOAT2));
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, VertexAttributeType type,
        const DirectX::XMFLOAT4& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMFLOAT4));
    }

    template<>
    void VertexData::SetAttribute<float>(size_t vertexIndex, VertexAttributeType type,
        const float& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(float));
    }

    template<>
    DirectX::XMFLOAT3 VertexData::GetAttribute<DirectX::XMFLOAT3>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        DirectX::XMFLOAT3 result;
        memcpy(&result, attrStart, sizeof(DirectX::XMFLOAT3));
        return result;
    }

    template<>
    DirectX::XMFLOAT2 VertexData::GetAttribute<DirectX::XMFLOAT2>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        DirectX::XMFLOAT2 result;
        memcpy(&result, attrStart, sizeof(DirectX::XMFLOAT2));
        return result;
    }

    template<>
    DirectX::XMFLOAT4 VertexData::GetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        DirectX::XMFLOAT4 result;
        memcpy(&result, attrStart, sizeof(DirectX::XMFLOAT4));
        return result;
    }

}