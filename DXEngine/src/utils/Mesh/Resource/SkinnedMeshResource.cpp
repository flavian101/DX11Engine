#include "dxpch.h"
#include "SkinnedMeshResource.h"

namespace DXEngine
{
	bool SkinnedMeshResource::ValidateBoneWeights() const
	{
        if (!m_Skeleton || !GetVertexData())
            return false;

        const VertexLayout& layout = GetVertexData()->GetLayout();

        // Check if we have bone indices and weights
        if (!layout.HasAttribute(VertexAttributeType::BlendIndices) ||
            !layout.HasAttribute(VertexAttributeType::BlendWeights))
        {
            return false;
        }

        // Validate that bone indices reference valid bones in skeleton
        size_t vertexCount = GetVertexData()->GetVertexCount();
        for (size_t i = 0; i < vertexCount; ++i)
        {
            auto indices = GetVertexData()->GetAttribute<DirectX::XMINT4>(i, VertexAttributeType::BlendIndices);

            // Check each bone index
            if (indices.x >= 0 && indices.x >= static_cast<int>(m_Skeleton->GetBoneCount()))
                return false;
            if (indices.y >= 0 && indices.y >= static_cast<int>(m_Skeleton->GetBoneCount()))
                return false;
            if (indices.z >= 0 && indices.z >= static_cast<int>(m_Skeleton->GetBoneCount()))
                return false;
            if (indices.w >= 0 && indices.w >= static_cast<int>(m_Skeleton->GetBoneCount()))
                return false;
        }

        return true;
    }
}