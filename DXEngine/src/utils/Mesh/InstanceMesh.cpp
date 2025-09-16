#include "dxpch.h"
#include "InstanceMesh.h"
#include "Utils/Mesh/Resource/InstanceMeshResource.h"

namespace DXEngine
{
    InstancedMesh::InstancedMesh(std::shared_ptr<InstancedMeshResource> resource)
        : Mesh(resource)
        , m_InstanceDataDirty(true)
    {
    }

    void InstancedMesh::DrawInstanced(size_t submeshIndex) const
    {
        auto instancedResource = GetInstancedResource();
        if (!instancedResource)
            return;

        uint32_t instanceCount = static_cast<uint32_t>(instancedResource->GetInstanceCount());
        if (instanceCount == 0)
            return;

        UpdateInstanceData();
        Mesh::DrawInstanced(instanceCount, submeshIndex);
    }

    void InstancedMesh::DrawAllInstanced() const
    {
        auto instancedResource = GetInstancedResource();
        if (!instancedResource)
            return;

        uint32_t instanceCount = static_cast<uint32_t>(instancedResource->GetInstanceCount());
        if (instanceCount == 0)
            return;

        UpdateInstanceData();

        if (GetResource()->HasSubmeshes())
        {
            for (size_t i = 0; i < GetSubmeshCount(); ++i)
            {
                Mesh::DrawInstanced(instanceCount, i);
            }
        }
        else
        {
            Mesh::DrawInstanced(instanceCount, 0);
        }
    }

    bool InstancedMesh::UpdateInstanceData() const
    {
        if (!m_InstanceDataDirty)
            return true;

        auto instancedResource = GetInstancedResource();
        if (!instancedResource)
            return false;

        const VertexData* instanceData = instancedResource->GetInstanceData();
        if (!instanceData || instanceData->GetVertexCount() == 0)
            return false;

        // Create/update instance buffer
        const void* data = instanceData->GetVertexData();
        size_t dataSize = instanceData->GetDataSize();

        BufferDesc desc;
        desc.bufferType = BufferType::Vertex;
        desc.usageType = UsageType::Dynamic;
        desc.byteWidth = static_cast<UINT>(dataSize);
        desc.initialData = data;

        if (!m_InstanceBuffer->Initialize(desc))
        {
            OutputDebugStringA("Failed to create instance buffer\n");
            return false;
        }

        m_InstanceDataDirty = false;
        return true;
    }

    void InstancedMesh::OnResourceChanged()
    {
        Mesh::OnResourceChanged();
        m_InstanceDataDirty = true;
    }

}