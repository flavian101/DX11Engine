#include "dxpch.h"
#include "MeshBuffers.h"

namespace DXEngine
{

    MeshBuffers::~MeshBuffers()
    {
        Release();
    }

    bool MeshBuffers::CreateFromResource(RHI::IGraphicsDevice* device,const MeshResource& resource)
    {
        Release();

        const VertexData* vertexData = resource.GetVertexData();
        const IndexData* indexData = resource.GetIndexData();

        if (!vertexData || vertexData->GetVertexCount() == 0)
            return false;

        // Create vertex buffers for each slot
        const VertexLayout& layout = vertexData->GetLayout();
        std::unordered_map<uint32_t, bool> processedSlots;

        for (const auto& attr : layout.GetAttributes())
        {
            if (processedSlots[attr.Slot])
                continue;
            processedSlots[attr.Slot] = true;

            const void* data = vertexData->GetVertexData(attr.Slot);
            size_t dataSize = vertexData->GetDataSize(attr.Slot);
            uint32_t stride = layout.GetStride(attr.Slot);

            if (dataSize == 0)
                continue;

            RHI::BufferDesc bufferDesc;
            bufferDesc.type = RHI::BufferType::Vertex;
            bufferDesc.usage = RHI::BufferUsage::Static;  // Default for static mesh data
            bufferDesc.size = static_cast<uint32_t>(dataSize);
            bufferDesc.initialData = data;
            bufferDesc.debugName = resource.GetName() + "_VB_Slot" + std::to_string(attr.Slot);

            auto  vertexBuffer = device->CreateBuffer(bufferDesc);
            VertexBufferData vbData;
            vbData.buffer = std::move(vertexBuffer);
            vbData.stride = stride;
            vbData.offset = 0;

            m_VertexBuffers[attr.Slot] = std::move(vbData);
        }

        m_VertexCount = vertexData->GetVertexCount();

        // Create index buffer if available
        if (indexData && indexData->GetIndexCount() > 0)
        {
            if (!CreateIndexBuffer(device, indexData, resource.GetName()));
        }

        m_Topology = static_cast<PrimitiveTopology>(resource.GetTopology());
        return true;
    }

    bool MeshBuffers::CreateFromVertexData(RHI::IGraphicsDevice* device,const VertexData& vertexData, const IndexData* indexData)
    {
        // Create a temporary resource and use the existing method
        MeshResource tempResource;
        auto vertexDataCopy = std::make_unique<VertexData>(vertexData);
        const_cast<MeshResource&>(tempResource).SetVertexData(std::move(vertexDataCopy));

        if (indexData)
        {
            auto indexDataCopy = std::make_unique<IndexData>(*indexData);
            const_cast<MeshResource&>(tempResource).SetIndexData(std::move(indexDataCopy));
        }

        return CreateFromResource(device,tempResource);
    }

    bool MeshBuffers::AddVertexBuffer(RHI::IGraphicsDevice* device, const VertexData& vertexData, uint32_t slot)
    {
        const void* data = vertexData.GetVertexData(slot);
        size_t dataSize = vertexData.GetDataSize(slot);
        uint32_t stride = vertexData.GetLayout().GetStride(slot);

        if (dataSize == 0)
            return false;

        RHI::BufferDesc bufferDesc;
        bufferDesc.type = RHI::BufferType::Vertex;
        bufferDesc.usage = RHI::BufferUsage::Static;  // Default for static mesh data
        bufferDesc.size = static_cast<uint32_t>(dataSize);
        bufferDesc.initialData = data;
      //  bufferDesc.debugName = resource.GetName() + "_VB_Slot" + std::to_string(attr.Slot);

        auto vertexBuffer = device->CreateBuffer(bufferDesc);

        VertexBufferData vbData;
        vbData.buffer = std::move(vertexBuffer);
        vbData.stride = stride;
        vbData.offset = 0;
        m_VertexBuffers[slot] = std::move(vbData);
        return true;
    }

    void MeshBuffers::Bind(RHI::ICommandBuffer* cmd,uint32_t startSlot) const
    {
        BindVertexBuffers(cmd,startSlot);
        if (m_IndexBuffer)
            BindIndexBuffer(cmd);
    }

    void MeshBuffers::BindVertexBuffers(RHI::ICommandBuffer* cmd, uint32_t startSlot) const
    {
        if (m_VertexBuffers.empty() || !cmd)
            return;

        for (const auto& [slot, data] : m_VertexBuffers)
        {
            cmd->SetVertexBuffer(data.buffer.get(), startSlot + slot, data.offset);
        }
    }

    void MeshBuffers::BindIndexBuffer(RHI::ICommandBuffer* cmd) const
    {
        if (!m_IndexBuffer || !m_IndexBuffer->IsValid() || !cmd)
            return;

        cmd->SetIndexBuffer(m_IndexBuffer->buffer.get(), 0, 0);
    }

    void MeshBuffers::Release()
    {
        m_VertexBuffers.clear();
        m_IndexBuffer.reset();
        m_VertexCount = 0;
        m_IndexCount = 0;
    }

    bool MeshBuffers::IsValid() const
    {
        return !m_VertexBuffers.empty() && m_VertexCount > 0;
    }

    bool MeshBuffers::CreateIndexBuffer(RHI::IGraphicsDevice* device, const IndexData* indexData, const std::string& debugName)
    {
        if (!device || !indexData)
            return false;

        const void* indexPtr = indexData->GetData();
        if (!indexPtr)
            return false;

        m_IndexType = indexData->GetIndexType();
        m_IndexCount = indexData->GetIndexCount();

        // Calculate buffer size based on index type
        size_t indexSize = (m_IndexType == IndexType::UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);
        size_t bufferSize = m_IndexCount * indexSize;

        // Create single buffer
        RHI::BufferDesc bufferDesc;
        bufferDesc.type = RHI::BufferType::Index;
        bufferDesc.usage = RHI::BufferUsage::Static;
        bufferDesc.size = static_cast<uint32_t>(bufferSize);
        bufferDesc.stride = static_cast<uint32_t>(indexSize);
        bufferDesc.initialData = indexPtr;
        bufferDesc.debugName = debugName + "_IB_" +
            (m_IndexType == IndexType::UInt16 ? "16" : "32");

        m_IndexBuffer = std::make_unique<IndexBufferInfo>();
        m_IndexBuffer->buffer = device->CreateBuffer(bufferDesc);
        m_IndexBuffer->indexType = m_IndexType;

        if (!m_IndexBuffer->IsValid())
        {
            m_IndexBuffer.reset();
            return false;
        }

        return true;
    }

    size_t MeshBuffers::GetGPUMemoryUsage() const
    {
        size_t usage = 0;

        for (const auto& [slot, data] : m_VertexBuffers)
        {
            usage += data.buffer->GetMemoryUsage();
        }

        if (m_IndexBuffer && m_IndexBuffer->IsValid())
        {
            usage += m_IndexBuffer->buffer->GetMemoryUsage();
        }

        return usage;
    }

}