#include "dxpch.h"
#include "IndexData.h"
#include "MeshOptimizer.h"


namespace DXEngine
{
    void IndexData::SetIndexType(IndexType type)
    {
        if (m_IndexType == type)
            return;

        // Convert existing indices to new type if needed
        if (GetIndexCount() > 0)
        {
            std::vector<uint32_t> tempIndices;
            tempIndices.reserve(GetIndexCount());

            // Extract all indices as uint32_t
            for (size_t i = 0; i < GetIndexCount(); ++i)
            {
                tempIndices.push_back(GetIndex(i));
            }

            // Clear current data and set new type
            m_IndexType = type;
            Clear();

            // Add back the indices in the new format
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
            Clear(); // Initialize with correct type
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

    bool IndexData::OptimizeForCache(size_t vertexCount, size_t firstIndex, size_t indexCount)
    {
        const size_t total = GetIndexCount();
        if (firstIndex > total) return false;
        indexCount = std::min(indexCount, total - firstIndex);

        // work on a copy: a range that cannot be optimised (not whole triangles, an index past the vertex count)
        // must leave the buffer exactly as it was
        std::vector<uint32_t> range(indexCount);
        for (size_t i = 0; i < indexCount; ++i)
            range[i] = GetIndex(firstIndex + i);
        if (!MeshOpt::OptimizeVertexCache(range.data(), range.size(), vertexCount))
            return false;
        for (size_t i = 0; i < indexCount; ++i)
            SetIndex(firstIndex + i, range[i]);
        return true;
    }

}