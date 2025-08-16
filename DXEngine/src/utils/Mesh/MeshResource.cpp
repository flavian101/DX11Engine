#include "dxpch.h"
#include "MeshResource.h"
#include <algorithm>
#include <sstream>
#include <cassert>

namespace DXEngine
{
    // BoundingBox Implementation
    DirectX::XMFLOAT3 BoundingBox::GetCenter() const
    {
        return DirectX::XMFLOAT3(
            (min.x + max.x) * 0.5f,
            (min.y + max.y) * 0.5f,
            (min.z + max.z) * 0.5f
        );
    }

    DirectX::XMFLOAT3 BoundingBox::GetExtents() const
    {
        return DirectX::XMFLOAT3(
            (max.x - min.x) * 0.5f,
            (max.y - min.y) * 0.5f,
            (max.z - min.z) * 0.5f
        );
    }

    void BoundingBox::GetCorners(DirectX::XMVECTOR corners[8]) const
    {
        using namespace DirectX;

        // Load min and max into vectors
        XMVECTOR vMin = XMLoadFloat3(&min);
        XMVECTOR vMax = XMLoadFloat3(&max);

        // Extract min/max components
        float minX = XMVectorGetX(vMin);
        float minY = XMVectorGetY(vMin);
        float minZ = XMVectorGetZ(vMin);

        float maxX = XMVectorGetX(vMax);
        float maxY = XMVectorGetY(vMax);
        float maxZ = XMVectorGetZ(vMax);

        // Bottom face (y = minY)
        corners[0] = XMVectorSet(minX, minY, minZ, 0.0f); // Left-bottom-near
        corners[1] = XMVectorSet(maxX, minY, minZ, 0.0f); // Right-bottom-near
        corners[2] = XMVectorSet(maxX, minY, maxZ, 0.0f); // Right-bottom-far
        corners[3] = XMVectorSet(minX, minY, maxZ, 0.0f); // Left-bottom-far

        // Top face (y = maxY)
        corners[4] = XMVectorSet(minX, maxY, minZ, 0.0f); // Left-top-near
        corners[5] = XMVectorSet(maxX, maxY, minZ, 0.0f); // Right-top-near
        corners[6] = XMVectorSet(maxX, maxY, maxZ, 0.0f); // Right-top-far
        corners[7] = XMVectorSet(minX, maxY, maxZ, 0.0f); // Left-top-far
    }

    float BoundingBox::GetRadius() const
    {
        DirectX::XMFLOAT3 extents = GetExtents();
        return sqrtf(extents.x * extents.x + extents.y * extents.y + extents.z * extents.z);
    }

    void BoundingBox::Expand(const DirectX::XMFLOAT3& point)
    {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    }

    void BoundingBox::Expand(const BoundingBox& other)
    {
        Expand(other.min);
        Expand(other.max);
    }

    bool BoundingBox::Contains(const DirectX::XMFLOAT3& point) const
    {
        return point.x >= min.x && point.x <= max.x &&
            point.y >= min.y && point.y <= max.y &&
            point.z >= min.z && point.z <= max.z;
    }

    bool BoundingBox::Intersects(const BoundingBox& other) const
    {
        return !(other.min.x > max.x || other.max.x < min.x ||
            other.min.y > max.y || other.max.y < min.y ||
            other.min.z > max.z || other.max.z < min.z);
    }

    // BoundingSphere Implementation
    bool BoundingSphere::Contains(const DirectX::XMFLOAT3& point) const
    {
        float dx = point.x - center.x;
        float dy = point.y - center.y;
        float dz = point.z - center.z;
        float distanceSquared = dx * dx + dy * dy + dz * dz;
        return distanceSquared <= radius * radius;
    }

    bool BoundingSphere::Intersects(const BoundingSphere& other) const
    {
        float dx = center.x - other.center.x;
        float dy = center.y - other.center.y;
        float dz = center.z - other.center.z;
        float distanceSquared = dx * dx + dy * dy + dz * dz;
        float radiusSum = radius + other.radius;
        return distanceSquared <= radiusSum * radiusSum;
    }

    bool BoundingSphere::Intersects(const BoundingBox& box) const
    {
        // Find closest point on box to sphere center
        DirectX::XMFLOAT3 closest;
        closest.x = std::max(box.min.x, std::min(center.x, box.max.x));
        closest.y = std::max(box.min.y, std::min(center.y, box.max.y));
        closest.z = std::max(box.min.z, std::min(center.z, box.max.z));

        return Contains(closest);
    }

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

    // MeshResource Implementation
    void MeshResource::SetVertexData(std::unique_ptr<VertexData> vertexData)
    {
        m_VertexData = std::move(vertexData);
        InvalidateBounds();
        OnDataChanged();
    }

    void MeshResource::SetIndexData(std::unique_ptr<IndexData> indexData)
    {
        m_IndexData = std::move(indexData);
        OnDataChanged();
    }

    void MeshResource::AddSubMesh(const SubMesh& submesh)
    {
        m_SubMeshes.push_back(submesh);
    }

    void MeshResource::AddSubMesh(const std::string& name, uint32_t indexStart, uint32_t indexCount, uint32_t materialIndex)
    {
        SubMesh submesh(name, indexStart, indexCount, 0, 0, materialIndex);
        AddSubMesh(submesh);
    }

    void MeshResource::ComputeBounds()
    {
        if (!m_VertexData || m_VertexData->GetVertexCount() == 0)
        {
            m_BoundingBox = BoundingBox();
            m_BoundingSphere = BoundingSphere();
            m_BoundsDirty = false;
            return;
        }

        const VertexLayout& layout = m_VertexData->GetLayout();
        const VertexAttribute* posAttr = layout.FindAttribute(VertexAttributeType::Position);

        if (!posAttr)
        {
            m_BoundingBox = BoundingBox();
            m_BoundingSphere = BoundingSphere();
            m_BoundsDirty = false;
            return;
        }

        // Initialize bounds with first vertex
        auto firstPos = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(0, VertexAttributeType::Position);
        m_BoundingBox = BoundingBox(firstPos, firstPos);

        // Expand bounds with all vertices
        for (size_t i = 1; i < m_VertexData->GetVertexCount(); ++i)
        {
            auto pos = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i, VertexAttributeType::Position);
            m_BoundingBox.Expand(pos);
        }

        // Compute bounding sphere
        DirectX::XMFLOAT3 center = m_BoundingBox.GetCenter();
        float maxRadiusSquared = 0.0f;

        for (size_t i = 0; i < m_VertexData->GetVertexCount(); ++i)
        {
            auto pos = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i, VertexAttributeType::Position);
            float dx = pos.x - center.x;
            float dy = pos.y - center.y;
            float dz = pos.z - center.z;
            float radiusSquared = dx * dx + dy * dy + dz * dz;
            maxRadiusSquared = std::max(maxRadiusSquared, radiusSquared);
        }

        m_BoundingSphere = BoundingSphere(center, sqrtf(maxRadiusSquared));
        m_BoundsDirty = false;

        OnBoundsChanged();
    }

    void MeshResource::SetBounds(const BoundingBox& box, const BoundingSphere& sphere)
    {
        m_BoundingBox = box;
        m_BoundingSphere = sphere;
        m_BoundsDirty = false;
        OnBoundsChanged();
    }

    bool MeshResource::IsValid() const
    {
        if (!m_VertexData || m_VertexData->GetVertexCount() == 0)
            return false;

        if (!m_VertexData->IsValid())
            return false;

        // Check submeshes are valid
        for (const auto& submesh : m_SubMeshes)
        {
            if (m_IndexData)
            {
                if (submesh.indexStart + submesh.indexCount > m_IndexData->GetIndexCount())
                    return false;
            }
        }

        return true;
    }

    void MeshResource::GenerateNormals()
    {
        if (!m_VertexData || !m_IndexData)
            return;

        const VertexLayout& layout = m_VertexData->GetLayout();
        if (!layout.HasAttribute(VertexAttributeType::Position) ||
            !layout.HasAttribute(VertexAttributeType::Normal))
            return;

        // Zero out existing normals
        DirectX::XMFLOAT3 zeroNormal(0.0f, 0.0f, 0.0f);
        for (size_t i = 0; i < m_VertexData->GetVertexCount(); ++i)
        {
            m_VertexData->SetAttribute(i, VertexAttributeType::Normal, zeroNormal);
        }

        // Calculate face normals and accumulate
        size_t indexCount = m_IndexData->GetIndexCount();
        for (size_t i = 0; i < indexCount; i += 3)
        {
            uint32_t i0 = m_IndexData->GetIndex(i);
            uint32_t i1 = m_IndexData->GetIndex(i + 1);
            uint32_t i2 = m_IndexData->GetIndex(i + 2);

            auto p0 = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i0, VertexAttributeType::Position);
            auto p1 = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i1, VertexAttributeType::Position);
            auto p2 = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i2, VertexAttributeType::Position);

            // Calculate face normal
            DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&p0);
            DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&p1);
            DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&p2);

            DirectX::XMVECTOR edge1 = DirectX::XMVectorSubtract(v1, v0);
            DirectX::XMVECTOR edge2 = DirectX::XMVectorSubtract(v2, v0);
            DirectX::XMVECTOR normal = DirectX::XMVector3Cross(edge1, edge2);

            DirectX::XMFLOAT3 faceNormal;
            DirectX::XMStoreFloat3(&faceNormal, normal);

            // Accumulate to vertex normals
            auto n0 = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i0, VertexAttributeType::Normal);
            auto n1 = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i1, VertexAttributeType::Normal);
            auto n2 = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i2, VertexAttributeType::Normal);

            n0.x += faceNormal.x; n0.y += faceNormal.y; n0.z += faceNormal.z;
            n1.x += faceNormal.x; n1.y += faceNormal.y; n1.z += faceNormal.z;
            n2.x += faceNormal.x; n2.y += faceNormal.y; n2.z += faceNormal.z;

            m_VertexData->SetAttribute(i0, VertexAttributeType::Normal, n0);
            m_VertexData->SetAttribute(i1, VertexAttributeType::Normal, n1);
            m_VertexData->SetAttribute(i2, VertexAttributeType::Normal, n2);
        }

        // Normalize all normals
        for (size_t i = 0; i < m_VertexData->GetVertexCount(); ++i)
        {
            auto normal = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i, VertexAttributeType::Normal);
            DirectX::XMVECTOR normalVec = DirectX::XMLoadFloat3(&normal);
            normalVec = DirectX::XMVector3Normalize(normalVec);
            DirectX::XMStoreFloat3(&normal, normalVec);
            m_VertexData->SetAttribute(i, VertexAttributeType::Normal, normal);
        }

        OnDataChanged();
    }

    void MeshResource::GenerateTangents()
    {
        if (!m_VertexData || !m_IndexData)
            return;

        const VertexLayout& layout = m_VertexData->GetLayout();
        if (!layout.HasAttribute(VertexAttributeType::Position) ||
            !layout.HasAttribute(VertexAttributeType::Normal) ||
            !layout.HasAttribute(VertexAttributeType::Tangent) ||
            !layout.HasAttribute(VertexAttributeType::TexCoord0))
            return;

        size_t vertexCount = m_VertexData->GetVertexCount();

        // Initialize tangent vectors
        std::vector<DirectX::XMFLOAT3> tangents(vertexCount, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
        std::vector<DirectX::XMFLOAT3> bitangents(vertexCount, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

        // Calculate tangents using Lengyel's method
        size_t indexCount = m_IndexData->GetIndexCount();
        for (size_t i = 0; i < indexCount; i += 3)
        {
            uint32_t i0 = m_IndexData->GetIndex(i);
            uint32_t i1 = m_IndexData->GetIndex(i + 1);
            uint32_t i2 = m_IndexData->GetIndex(i + 2);

            auto p0 = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i0, VertexAttributeType::Position);
            auto p1 = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i1, VertexAttributeType::Position);
            auto p2 = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i2, VertexAttributeType::Position);

            auto uv0 = m_VertexData->GetAttribute<DirectX::XMFLOAT2>(i0, VertexAttributeType::TexCoord0);
            auto uv1 = m_VertexData->GetAttribute<DirectX::XMFLOAT2>(i1, VertexAttributeType::TexCoord0);
            auto uv2 = m_VertexData->GetAttribute<DirectX::XMFLOAT2>(i2, VertexAttributeType::TexCoord0);

            DirectX::XMFLOAT3 edge1(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
            DirectX::XMFLOAT3 edge2(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z);

            DirectX::XMFLOAT2 deltaUV1(uv1.x - uv0.x, uv1.y - uv0.y);
            DirectX::XMFLOAT2 deltaUV2(uv2.x - uv0.x, uv2.y - uv0.y);

            float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

            DirectX::XMFLOAT3 tangent(
                f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
                f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
                f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)
            );

            DirectX::XMFLOAT3 bitangent(
                f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x),
                f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y),
                f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z)
            );

            // Accumulate tangents
            tangents[i0].x += tangent.x; tangents[i0].y += tangent.y; tangents[i0].z += tangent.z;
            tangents[i1].x += tangent.x; tangents[i1].y += tangent.y; tangents[i1].z += tangent.z;
            tangents[i2].x += tangent.x; tangents[i2].y += tangent.y; tangents[i2].z += tangent.z;

            bitangents[i0].x += bitangent.x; bitangents[i0].y += bitangent.y; bitangents[i0].z += bitangent.z;
            bitangents[i1].x += bitangent.x; bitangents[i1].y += bitangent.y; bitangents[i1].z += bitangent.z;
            bitangents[i2].x += bitangent.x; bitangents[i2].y += bitangent.y; bitangents[i2].z += bitangent.z;
        }

        // Orthogonalize and normalize tangents
        for (size_t i = 0; i < vertexCount; ++i)
        {
            auto normal = m_VertexData->GetAttribute<DirectX::XMFLOAT3>(i, VertexAttributeType::Normal);

            DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&normal);
            DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&tangents[i]);
            DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&bitangents[i]);

            // Gram-Schmidt orthogonalize
            t = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(t, DirectX::XMVectorMultiply(n, DirectX::XMVector3Dot(n, t))));

            // Calculate handedness
            float handedness = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Cross(n, t), b)) < 0.0f ? -1.0f : 1.0f;

            DirectX::XMFLOAT4 finalTangent;
            DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&finalTangent), t);
            finalTangent.w = handedness;

            m_VertexData->SetAttribute(i, VertexAttributeType::Tangent, finalTangent);
        }

        OnDataChanged();
    }

    void MeshResource::GenerateBounds()
    {
        ComputeBounds();
    }

    void MeshResource::OptimizeForRendering()
    {
        if (m_IndexData)
        {
            m_IndexData->OptimizeForCache();
        }

        // Additional optimizations could be added here:
        // - Vertex deduplication
        // - Vertex fetch optimization
        // - LOD generation

        OnDataChanged();
    }

    size_t MeshResource::GetMemoryUsage() const
    {
        size_t usage = 0;

        if (m_VertexData)
        {
            for (const auto& attr : m_VertexData->GetLayout().GetAttributes())
            {
                usage += m_VertexData->GetDataSize(attr.Slot);
            }
        }

        if (m_IndexData)
        {
            usage += m_IndexData->GetDataSize();
        }

        usage += m_SubMeshes.size() * sizeof(SubMesh);

        return usage;
    }

    std::string MeshResource::GetDebugInfo() const
    {
        std::ostringstream oss;
        oss << "MeshResource: " << m_Name << "\n";
        oss << "Vertices: " << (m_VertexData ? m_VertexData->GetVertexCount() : 0) << "\n";
        oss << "Indices: " << (m_IndexData ? m_IndexData->GetIndexCount() : 0) << "\n";
        oss << "Submeshes: " << m_SubMeshes.size() << "\n";
        oss << "Topology: " << static_cast<int>(m_Topology) << "\n";
        oss << "Memory: " << GetMemoryUsage() << " bytes\n";

        if (m_VertexData)
        {
            oss << "Layout:\n" << m_VertexData->GetLayout().GetDebugString();
        }

        return oss.str();
    }

    void MeshResource::InvalidateBounds()
    {
        m_BoundsDirty = true;
    }

    // Factory methods
    std::unique_ptr<MeshResource> MeshResource::CreateQuad(const std::string& name, float width, float height)
    {
        auto layout = VertexLayout::CreateBasic();
        auto vertexData = std::make_unique<VertexData>(layout);
        vertexData->Resize(4);

        float halfWidth = width * 0.5f;
        float halfHeight = height * 0.5f;

        // Positions
        vertexData->SetAttribute(0, VertexAttributeType::Position, DirectX::XMFLOAT3(-halfWidth, -halfHeight, 0.0f));
        vertexData->SetAttribute(1, VertexAttributeType::Position, DirectX::XMFLOAT3(halfWidth, -halfHeight, 0.0f));
        vertexData->SetAttribute(2, VertexAttributeType::Position, DirectX::XMFLOAT3(halfWidth, halfHeight, 0.0f));
        vertexData->SetAttribute(3, VertexAttributeType::Position, DirectX::XMFLOAT3(-halfWidth, halfHeight, 0.0f));

        // Normals
        DirectX::XMFLOAT3 normal(0.0f, 0.0f, 1.0f);
        for (int i = 0; i < 4; ++i)
        {
            vertexData->SetAttribute(i, VertexAttributeType::Normal, normal);
        }

        // Texture coordinates
        vertexData->SetAttribute(0, VertexAttributeType::TexCoord0, DirectX::XMFLOAT2(0.0f, 1.0f));
        vertexData->SetAttribute(1, VertexAttributeType::TexCoord0, DirectX::XMFLOAT2(1.0f, 1.0f));
        vertexData->SetAttribute(2, VertexAttributeType::TexCoord0, DirectX::XMFLOAT2(1.0f, 0.0f));
        vertexData->SetAttribute(3, VertexAttributeType::TexCoord0, DirectX::XMFLOAT2(0.0f, 0.0f));

        auto indexData = std::make_unique<IndexData>(IndexType::UInt16);
        indexData->AddTriangle(0, 1, 2);
        indexData->AddTriangle(2, 3, 0);

        auto resource = std::make_unique<MeshResource>(name);
        resource->SetVertexData(std::move(vertexData));
        resource->SetIndexData(std::move(indexData));
        resource->SetTopology(PrimitiveTopology::TriangleList);

        return resource;
    }
   
    std::unique_ptr<MeshResource> MeshResource::CreateCube(const std::string& name, float size)
    {
        auto layout = VertexLayout::CreateBasic();
        auto vertexData = std::make_unique<VertexData>(layout);
        vertexData->Resize(24); // 6 faces * 4 vertices per face

        float half = size * 0.5f;

        // Cube vertices (6 faces, 4 vertices each)
        DirectX::XMFLOAT3 positions[24] = {
            // Front face
            {-half, -half,  half}, { half, -half,  half}, { half,  half,  half}, {-half,  half,  half},
            // Back face
            { half, -half, -half}, {-half, -half, -half}, {-half,  half, -half}, { half,  half, -half},
            // Left face
            {-half, -half, -half}, {-half, -half,  half}, {-half,  half,  half}, {-half,  half, -half},
            // Right face
            { half, -half,  half}, { half, -half, -half}, { half,  half, -half}, { half,  half,  half},
            // Top face
            {-half,  half,  half}, { half,  half,  half}, { half,  half, -half}, {-half,  half, -half},
            // Bottom face
            {-half, -half, -half}, { half, -half, -half}, { half, -half,  half}, {-half, -half,  half}
        };

        DirectX::XMFLOAT3 normals[24] = {
            // Front face
            {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1},
            // Back face
            {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
            // Left face
            {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0},
            // Right face
            {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0},
            // Top face
            {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0},
            // Bottom face
            {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}
        };

        DirectX::XMFLOAT2 texCoords[4] = {
            {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}
        };

        // Set vertex data
        for (int i = 0; i < 24; ++i)
        {
            vertexData->SetAttribute(i, VertexAttributeType::Position, positions[i]);
            vertexData->SetAttribute(i, VertexAttributeType::Normal, normals[i]);
            vertexData->SetAttribute(i, VertexAttributeType::TexCoord0, texCoords[i % 4]);
        }

        // Create indices (6 faces * 2 triangles * 3 vertices = 36 indices)
        auto indexData = std::make_unique<IndexData>(IndexType::UInt16);
        for (uint16_t face = 0; face < 6; ++face)
        {
            uint16_t offset = face * 4;
            indexData->AddTriangle(offset + 0, offset + 1, offset + 2);
            indexData->AddTriangle(offset + 2, offset + 3, offset + 0);
        }

        auto resource = std::make_unique<MeshResource>(name);
        resource->SetVertexData(std::move(vertexData));
        resource->SetIndexData(std::move(indexData));
        resource->SetTopology(PrimitiveTopology::TriangleList);

        return resource;
    }

    std::unique_ptr<MeshResource> MeshResource::CreateSphere(const std::string& name, float radius, uint32_t segments)
    {
        auto layout = VertexLayout::CreateBasic();

        uint32_t rings = segments / 2;
        uint32_t vertexCount = (rings + 1) * (segments + 1);

        auto vertexData = std::make_unique<VertexData>(layout);
        vertexData->Resize(vertexCount);

        // Generate vertices
        uint32_t vertexIndex = 0;
        for (uint32_t ring = 0; ring <= rings; ++ring)
        {
            float phi = static_cast<float>(ring) / rings * DirectX::XM_PI;
            float y = radius * cosf(phi);
            float ringRadius = radius * sinf(phi);

            for (uint32_t segment = 0; segment <= segments; ++segment)
            {
                float theta = static_cast<float>(segment) / segments * DirectX::XM_2PI;
                float x = ringRadius * cosf(theta);
                float z = ringRadius * sinf(theta);

                DirectX::XMFLOAT3 position(x, y, z);
                DirectX::XMFLOAT3 normal = position;
                DirectX::XMStoreFloat3(&normal, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&normal)));

                DirectX::XMFLOAT2 texCoord(
                    static_cast<float>(segment) / segments,
                    static_cast<float>(ring) / rings
                );

                vertexData->SetAttribute(vertexIndex, VertexAttributeType::Position, position);
                vertexData->SetAttribute(vertexIndex, VertexAttributeType::Normal, normal);
                vertexData->SetAttribute(vertexIndex, VertexAttributeType::TexCoord0, texCoord);

                ++vertexIndex;
            }
        }

        // Generate indices
        auto indexData = std::make_unique<IndexData>(IndexType::UInt32);
        for (uint32_t ring = 0; ring < rings; ++ring)
        {
            for (uint32_t segment = 0; segment < segments; ++segment)
            {
                uint32_t current = ring * (segments + 1) + segment;
                uint32_t next = current + segments + 1;

                indexData->AddTriangle(current, next, current + 1);
                indexData->AddTriangle(current + 1, next, next + 1);
            }
        }

        auto resource = std::make_unique<MeshResource>(name);
        resource->SetVertexData(std::move(vertexData));
        resource->SetIndexData(std::move(indexData));
        resource->SetTopology(PrimitiveTopology::TriangleList);

        return resource;
    }

    std::unique_ptr<MeshResource> MeshResource::CreateCylinder(const std::string& name, float radius, float height, uint32_t segments)
    {
        auto layout = VertexLayout::CreateBasic();

        // Vertices: segments+1 for each cap, segments+1 for each of 2 rings = (segments+1)*4
        uint32_t vertexCount = (segments + 1) * 4;

        auto vertexData = std::make_unique<VertexData>(layout);
        vertexData->Resize(vertexCount);

        float halfHeight = height * 0.5f;
        uint32_t vertexIndex = 0;

        // Bottom cap center
        vertexData->SetAttribute(vertexIndex, VertexAttributeType::Position, DirectX::XMFLOAT3(0.0f, -halfHeight, 0.0f));
        vertexData->SetAttribute(vertexIndex, VertexAttributeType::Normal, DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f));
        vertexData->SetAttribute(vertexIndex, VertexAttributeType::TexCoord0, DirectX::XMFLOAT2(0.5f, 0.5f));
        ++vertexIndex;

        // Top cap center
        vertexData->SetAttribute(vertexIndex, VertexAttributeType::Position, DirectX::XMFLOAT3(0.0f, halfHeight, 0.0f));
        vertexData->SetAttribute(vertexIndex, VertexAttributeType::Normal, DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f));
        vertexData->SetAttribute(vertexIndex, VertexAttributeType::TexCoord0, DirectX::XMFLOAT2(0.5f, 0.5f));
        ++vertexIndex;

        // Side vertices (bottom and top rings)
        for (uint32_t segment = 0; segment <= segments; ++segment)
        {
            float theta = static_cast<float>(segment) / segments * DirectX::XM_2PI;
            float x = radius * cosf(theta);
            float z = radius * sinf(theta);

            DirectX::XMFLOAT3 normal(x / radius, 0.0f, z / radius);
            DirectX::XMFLOAT2 texCoord(static_cast<float>(segment) / segments, 0.0f);

            // Bottom ring
            vertexData->SetAttribute(vertexIndex, VertexAttributeType::Position, DirectX::XMFLOAT3(x, -halfHeight, z));
            vertexData->SetAttribute(vertexIndex, VertexAttributeType::Normal, normal);
            vertexData->SetAttribute(vertexIndex, VertexAttributeType::TexCoord0, texCoord);
            ++vertexIndex;

            // Top ring
            texCoord.y = 1.0f;
            vertexData->SetAttribute(vertexIndex, VertexAttributeType::Position, DirectX::XMFLOAT3(x, halfHeight, z));
            vertexData->SetAttribute(vertexIndex, VertexAttributeType::Normal, normal);
            vertexData->SetAttribute(vertexIndex, VertexAttributeType::TexCoord0, texCoord);
            ++vertexIndex;
        }

        // Generate indices
        auto indexData = std::make_unique<IndexData>(IndexType::UInt32);

        // Side faces
        for (uint32_t segment = 0; segment < segments; ++segment)
        {
            uint32_t bottomCurrent = 2 + segment * 2;
            uint32_t topCurrent = bottomCurrent + 1;
            uint32_t bottomNext = bottomCurrent + 2;
            uint32_t topNext = topCurrent + 2;

            indexData->AddTriangle(bottomCurrent, topCurrent, bottomNext);
            indexData->AddTriangle(bottomNext, topCurrent, topNext);
        }

        // Bottom cap (triangle fan)
        for (uint32_t segment = 0; segment < segments; ++segment)
        {
            uint32_t current = 2 + segment * 2;
            uint32_t next = current + 2;
            indexData->AddTriangle(0, next, current); // Note: reversed winding for bottom face
        }

        // Top cap (triangle fan)
        for (uint32_t segment = 0; segment < segments; ++segment)
        {
            uint32_t current = 3 + segment * 2;
            uint32_t next = current + 2;
            indexData->AddTriangle(1, current, next);
        }

        auto resource = std::make_unique<MeshResource>(name);
        resource->SetVertexData(std::move(vertexData));
        resource->SetIndexData(std::move(indexData));
        resource->SetTopology(PrimitiveTopology::TriangleList);

        return resource;
    }

    std::unique_ptr<MeshResource> MeshResource::CreatePlane(const std::string& name, float width, float depth, uint32_t widthSegments, uint32_t depthSegments)
    {
        auto layout = VertexLayout::CreateBasic();

        uint32_t vertexCount = (widthSegments + 1) * (depthSegments + 1);
        auto vertexData = std::make_unique<VertexData>(layout);
        vertexData->Resize(vertexCount);

        float halfWidth = width * 0.5f;
        float halfDepth = depth * 0.5f;

        // Generate vertices
        uint32_t vertexIndex = 0;
        for (uint32_t z = 0; z <= depthSegments; ++z)
        {
            for (uint32_t x = 0; x <= widthSegments; ++x)
            {
                float px = (static_cast<float>(x) / widthSegments - 0.5f) * width;
                float pz = (static_cast<float>(z) / depthSegments - 0.5f) * depth;

                DirectX::XMFLOAT3 position(px, 0.0f, pz);
                DirectX::XMFLOAT3 normal(0.0f, 1.0f, 0.0f);
                DirectX::XMFLOAT2 texCoord(
                    static_cast<float>(x) / widthSegments,
                    static_cast<float>(z) / depthSegments
                );

                vertexData->SetAttribute(vertexIndex, VertexAttributeType::Position, position);
                vertexData->SetAttribute(vertexIndex, VertexAttributeType::Normal, normal);
                vertexData->SetAttribute(vertexIndex, VertexAttributeType::TexCoord0, texCoord);

                ++vertexIndex;
            }
        }

        // Generate indices
        auto indexData = std::make_unique<IndexData>(IndexType::UInt32);
        for (uint32_t z = 0; z < depthSegments; ++z)
        {
            for (uint32_t x = 0; x < widthSegments; ++x)
            {
                uint32_t topLeft = z * (widthSegments + 1) + x;
                uint32_t topRight = topLeft + 1;
                uint32_t bottomLeft = topLeft + (widthSegments + 1);
                uint32_t bottomRight = bottomLeft + 1;

                indexData->AddTriangle(topLeft, bottomLeft, topRight);
                indexData->AddTriangle(topRight, bottomLeft, bottomRight);
            }
        }

        auto resource = std::make_unique<MeshResource>(name);
        resource->SetVertexData(std::move(vertexData));
        resource->SetIndexData(std::move(indexData));
        resource->SetTopology(PrimitiveTopology::TriangleList);

        return resource;
    }

    // SkinnedMeshResource Implementation
    void SkinnedMeshResource::AddBone(const BoneInfo& bone)
    {
        m_Bones.push_back(bone);
    }

    int32_t SkinnedMeshResource::FindBoneIndex(const std::string& name) const
    {
        for (size_t i = 0; i < m_Bones.size(); ++i)
        {
            if (m_Bones[i].name == name)
                return static_cast<int32_t>(i);
        }
        return -1;
    }

    // InstancedMeshResource Implementation
    void InstancedMeshResource::SetInstanceLayout(const VertexLayout& layout)
    {
        m_InstanceLayout = layout;
        if (!layout.IsFinalized())
        {
            m_InstanceLayout.Finalize();
        }
    }

    void InstancedMeshResource::SetInstanceData(std::unique_ptr<VertexData> instanceData)
    {
        m_InstanceData = std::move(instanceData);
    }

    size_t InstancedMeshResource::GetInstanceCount() const
    {
        return m_InstanceData ? m_InstanceData->GetVertexCount() : 0;
    }


}