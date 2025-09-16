#include "dxpch.h"
#include "InstanceMeshResource.h"


namespace DXEngine
{
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