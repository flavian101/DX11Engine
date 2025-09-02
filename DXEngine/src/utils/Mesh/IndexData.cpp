#include "dxpch.h"
#include "IndexData.h"


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
    //     size_t triangleCount = GetIndexCount() / 3;
    //     if (GetIndexCount() % 3 != 0)
    //     {
    //         OutputDebugStringA("Warning: Index count is not a multiple of 3, cannot generate adjacency\n");
    //         return;
    //     }
    //
    //     adjacency.clear();
    //     adjacency.reserve(GetIndexCount() * 2); // Each edge can have one adjacent triangle
    //
    //     // Build edge-to-triangle mapping
    //     struct Edge
    //     {
    //         uint32_t v0, v1;
    //         size_t triangle;
    //         int edgeIndex; // 0, 1, or 2 for the edge within the triangle
    //
    //         Edge(uint32_t a, uint32_t b, size_t tri, int edge)
    //             : v0(std::min(a, b)), v1(std::max(a, b)), triangle(tri), edgeIndex(edge) {
    //         }
    //
    //         bool operator<(const Edge& other) const
    //         {
    //             if (v0 != other.v0) return v0 < other.v0;
    //             return v1 < other.v1;
    //         }
    //
    //         bool matches(const Edge& other) const
    //         {
    //             return v0 == other.v0 && v1 == other.v1;
    //         }
    //     };
    //
    //     std::vector<Edge> edges;
    //     edges.reserve(triangleCount * 3);
    //
    //     // Collect all edges
    //     for (size_t tri = 0; tri < triangleCount; ++tri)
    //     {
    //         uint32_t i0 = GetIndex(tri * 3 + 0);
    //         uint32_t i1 = GetIndex(tri * 3 + 1);
    //         uint32_t i2 = GetIndex(tri * 3 + 2);
    //
    //         edges.emplace_back(i0, i1, tri, 0);
    //         edges.emplace_back(i1, i2, tri, 1);
    //         edges.emplace_back(i2, i0, tri, 2);
    //     }
    //
    //     // Sort edges to find adjacencies
    //     std::sort(edges.begin(), edges.end());
    //
    //     // Build adjacency list
    //     std::vector<std::array<uint32_t, 3>> triangleAdjacency(triangleCount, { UINT32_MAX, UINT32_MAX, UINT32_MAX });
    //
    //     // Find adjacent triangles
    //     for (size_t i = 0; i < edges.size() - 1; ++i)
    //     {
    //         const Edge& edge1 = edges[i];
    //         const Edge& edge2 = edges[i + 1];
    //
    //         if (edge1.matches(edge2) && edge1.triangle != edge2.triangle)
    //         {
    //             // These triangles are adjacent
    //             triangleAdjacency[edge1.triangle][edge1.edgeIndex] = static_cast<uint32_t>(edge2.triangle);
    //             triangleAdjacency[edge2.triangle][edge2.edgeIndex] = static_cast<uint32_t>(edge1.triangle);
    //         }
    //     }
    //
    //     // Generate adjacency index buffer
    //     // Format: for each triangle (v0, v1, v2), adjacency is (v0, adj01, v1, adj12, v2, adj20)
    //     for (size_t tri = 0; tri < triangleCount; ++tri)
    //     {
    //         uint32_t i0 = GetIndex(tri * 3 + 0);
    //         uint32_t i1 = GetIndex(tri * 3 + 1);
    //         uint32_t i2 = GetIndex(tri * 3 + 2);
    //
    //         // Add triangle vertices with their adjacent triangle vertices
    //         adjacency.push_back(i0);
    //
    //         // Find the vertex opposite to edge (i0,i1) in adjacent triangle
    //         if (triangleAdjacency[tri][0] != UINT32_MAX)
    //         {
    //             size_t adjTri = triangleAdjacency[tri][0];
    //             uint32_t ai0 = GetIndex(adjTri * 3 + 0);
    //             uint32_t ai1 = GetIndex(adjTri * 3 + 1);
    //             uint32_t ai2 = GetIndex(adjTri * 3 + 2);
    //
    //             // Find vertex that's not i0 or i1
    //             if (ai0 != i0 && ai0 != i1) adjacency.push_back(ai0);
    //             else if (ai1 != i0 && ai1 != i1) adjacency.push_back(ai1);
    //             else adjacency.push_back(ai2);
    //         }
    //         else
    //         {
    //             adjacency.push_back(UINT32_MAX); // No adjacent triangle
    //         }
    //
    //         adjacency.push_back(i1);
    //
    //         // Adjacent vertex for edge (i1,i2)
    //         if (triangleAdjacency[tri][1] != UINT32_MAX)
    //         {
    //             size_t adjTri = triangleAdjacency[tri][1];
    //             uint32_t ai0 = GetIndex(adjTri * 3 + 0);
    //             uint32_t ai1 = GetIndex(adjTri * 3 + 1);
    //             uint32_t ai2 = GetIndex(adjTri * 3 + 2);
    //
    //             if (ai0 != i1 && ai0 != i2) adjacency.push_back(ai0);
    //             else if (ai1 != i1 && ai1 != i2) adjacency.push_back(ai1);
    //             else adjacency.push_back(ai2);
    //         }
    //         else
    //         {
    //             adjacency.push_back(UINT32_MAX);
    //         }
    //
    //         adjacency.push_back(i2);
    //
    //         // Adjacent vertex for edge (i2,i0)
    //         if (triangleAdjacency[tri][2] != UINT32_MAX)
    //         {
    //             size_t adjTri = triangleAdjacency[tri][2];
    //             uint32_t ai0 = GetIndex(adjTri * 3 + 0);
    //             uint32_t ai1 = GetIndex(adjTri * 3 + 1);
    //             uint32_t ai2 = GetIndex(adjTri * 3 + 2);
    //
    //             if (ai0 != i2 && ai0 != i0) adjacency.push_back(ai0);
    //             else if (ai1 != i2 && ai1 != i0) adjacency.push_back(ai1);
    //             else adjacency.push_back(ai2);
    //         }
    //         else
    //         {
    //             adjacency.push_back(UINT32_MAX);
    //         }
    //     }
    //
    //     char debugMsg[256];
    //     sprintf_s(debugMsg, "IndexData::GenerateAdjacency - Generated adjacency for %zu triangles\n", triangleCount);
    //     OutputDebugStringA(debugMsg);
    // }

}