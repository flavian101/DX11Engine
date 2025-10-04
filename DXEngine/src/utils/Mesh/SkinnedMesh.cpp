#include "dxpch.h"
#include "SkinnedMesh.h"
#include "Utils/Mesh/Resource/SkinnedMeshResource.h"


namespace DXEngine
{

    SkinnedMesh::SkinnedMesh(std::shared_ptr<SkinnedMeshResource> resource)
        : Mesh(resource)
    {
        //initialize Bone Matrices to identity if we have a skeleton
        if (auto skeleton = GetSkeleton())
        {
            m_BoneMatrices.resize(skeleton->GetBoneCount());
            for (auto& mat : m_BoneMatrices)
            {
                DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixIdentity());
            }
            CreateBoneBuffer();
        }
    }

    void SkinnedMesh::SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices)
    {
        if (matrices.size() > 128)
        {
            OutputDebugStringA("Warning: Too many bone matrices. Truncating to maximum.\n");
            m_BoneMatrices.assign(matrices.begin(), matrices.begin() + 128);
        }
        else
        {
            m_BoneMatrices = matrices;
        }

        m_BoneBufferDirty = true;

        // Create buffer if it doesn't exist
        if (!m_BoneBuffer && !m_BoneMatrices.empty())
        {
            CreateBoneBuffer();
        }

    }

    std::shared_ptr<Skeleton> SkinnedMesh::GetSkeleton() const
    {
        auto skinnedResource = GetSkinnedResource();

        return skinnedResource ? skinnedResource->GetSkeleton(): nullptr;
    }

    void SkinnedMesh::BindBoneData() const
    {
        if (m_BoneBufferDirty && !m_BoneMatrices.empty())
        {
            const_cast<SkinnedMesh*>(this)->UpdateBoneBuffer();
        }

        if (m_BoneBuffer && m_BoneBuffer->IsValid())
        {
            ID3D11Buffer* buffer = m_BoneBuffer->GetBuffer();
            RenderCommand::GetContext()->VSSetConstantBuffers(CB_Bones, 1, &buffer);
        }
    }
    void SkinnedMesh::Bind(const void* shaderByteCode, size_t byteCodeLength) const
    {
        Mesh::Bind(shaderByteCode, byteCodeLength);

        if (HasSkinningData())
        {
            BindBoneData();
        }
    }
    bool SkinnedMesh::HasSkinningData() const
    {
        if (m_BoneMatrices.empty())
            return false;
        auto resource = GetResource();
        if (!resource || !resource->GetVertexData())
            return false;

        const VertexLayout& layout = resource->GetVertexData()->GetLayout();
        return layout.HasAttribute(VertexAttributeType::BlendIndices)&&
            layout.HasAttribute(VertexAttributeType::BlendWeights);
    }
    void SkinnedMesh::OnResourceChanged()
    {
        Mesh::OnResourceChanged();
        //Reinitialize boneData if Skeleton exists
        if (auto skeleton = GetSkeleton())
        {
            m_BoneMatrices.resize(skeleton->GetBoneCount());
            for (auto& mat : m_BoneMatrices)
            {
                DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixIdentity());
            }
            CreateBoneBuffer();
            m_BoneBufferDirty = true;
        }
    }
    void SkinnedMesh::CreateBoneBuffer()
    {
        if (m_BoneMatrices.empty())
            return;

        // Create a constant buffer for bone matrices
        BufferDesc desc;
        desc.bufferType = BufferType::Constant;
        desc.usageType = UsageType::Dynamic;
        size_t matrixCount = std::min(m_BoneMatrices.size(),(size_t)128);
        desc.byteWidth = static_cast<UINT>(sizeof(DirectX::XMFLOAT4X4) * matrixCount);
        desc.byteWidth = desc.byteWidth;
        desc.initialData = m_BoneMatrices.data();

        m_BoneBuffer = std::make_unique<RawBuffer>();
        if (!m_BoneBuffer->Initialize(desc))
        {
            OutputDebugStringA("Failed to create bone buffer\n");
            m_BoneBuffer.reset();
        }
    }
    void SkinnedMesh::UpdateBoneBuffer()
    {
        if (!m_BoneBuffer || m_BoneMatrices.empty())
            return;

        size_t matrixCount = std::min(m_BoneMatrices.size(), (size_t)128);

        m_BoneBuffer->Update(m_BoneMatrices.data(),
            sizeof(DirectX::XMFLOAT4X4) * matrixCount);

        m_BoneBufferDirty = false;
    }
}