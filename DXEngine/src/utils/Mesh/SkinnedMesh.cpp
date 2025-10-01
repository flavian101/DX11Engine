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

        if (matrices.empty())
            return;

        // For skeletal animation, we need a structured buffer or array constant buffer
        if (!m_BoneBuffer)
        {
            // Create a buffer large enough for all bone matrices
            BufferDesc desc;
            desc.bufferType = BufferType::Constant;
            desc.usageType = UsageType::Dynamic;
            desc.byteWidth = static_cast<UINT>(sizeof(DirectX::XMFLOAT4X4) * matrices.size());
            desc.initialData = matrices.data();

            m_BoneBuffer = std::make_unique<RawBuffer>();
            if (!m_BoneBuffer->Initialize(desc))
            {
                OutputDebugStringA("Failed to create bone buffer\n");
                m_BoneBuffer.reset();
                return;
            }
        }
        else
        {
            // Update existing buffer
            m_BoneBuffer->Update(matrices.data(),
                sizeof(DirectX::XMFLOAT4X4) * matrices.size());
        }
    }

    void SkinnedMesh::BindBoneData() const
    {
        if (m_BoneBuffer && m_BoneBuffer->IsValid())
        {
            RenderCommand::GetContext()->VSSetConstantBuffers(CB_Bones, 1, m_BoneBuffer->GetAddressOf());
        }
    }
}