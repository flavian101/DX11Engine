#include "dxpch.h"
#include "SkinnedMesh.h"
#include "Utils/Mesh/Resource/SkinnedMeshResource.h"


namespace DXEngine
{

    SkinnedMesh::SkinnedMesh(std::shared_ptr<SkinnedMeshResource> resource)
        : Mesh(resource)
    {
    }

    void SkinnedMesh::SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices)
    {
        m_BoneMatrices = matrices;

        // Create or update GPU buffer using ConstantBuffer
        if (!matrices.empty())
        {
            if (!m_BoneBuffer)
            {
                m_BoneBuffer = std::make_unique<ConstantBuffer<DirectX::XMFLOAT4X4>>();
            }

            // For multiple matrices, we need to use UpdateArray
            // But ConstantBuffer is designed for single objects, so we need to create a buffer that can hold all matrices
            // For now, we'll recreate the buffer each time - this could be optimized
            if (matrices.size() == 1)
            {
                if (!m_BoneBuffer->Initialize(&matrices[0], UsageType::Dynamic))
                {
                    OutputDebugStringA("Failed to create bone constant buffer\n");
                    m_BoneBuffer.reset();
                }
            }
            else
            {
                // For multiple bone matrices, we need a different approach
                // Create a structured buffer or use a larger constant buffer
                // For now, we'll use the first matrix only as an example
                OutputDebugStringA("Warning: Multiple bone matrices not fully supported with current ConstantBuffer implementation\n");
                if (!m_BoneBuffer->Initialize(&matrices[0], UsageType::Dynamic))
                {
                    OutputDebugStringA("Failed to create bone constant buffer\n");
                    m_BoneBuffer.reset();
                }
            }
        }
    }

    void SkinnedMesh::BindBoneData() const
    {
        if (m_BoneBuffer && m_BoneBuffer->IsValid())
        {
            ID3D11Buffer* buffer = m_BoneBuffer->GetBuffer();
            RenderCommand::GetContext()->VSSetConstantBuffers(1, 1, &buffer);
        }
    }
}