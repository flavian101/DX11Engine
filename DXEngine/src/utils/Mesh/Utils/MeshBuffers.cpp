#include "dxpch.h"
#include "MeshBuffers.h"

namespace DXEngine
{

    MeshBuffers::~MeshBuffers()
    {
        Release();
    }

    bool MeshBuffers::CreateFromResource(const MeshResource& resource)
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

            auto vertexBuffer = std::make_unique<RawBuffer>();
            BufferDesc bufferDesc;
            bufferDesc.bufferType = BufferType::Vertex;
            bufferDesc.usageType = UsageType::Immutable;  // Default for static mesh data
            bufferDesc.byteWidth = static_cast<UINT>(dataSize);
            bufferDesc.initialData = data;

            if (!vertexBuffer->Initialize(bufferDesc))
            {
                OutputDebugStringA(("Failed to create vertex buffer for slot " + std::to_string(attr.Slot) + "\n").c_str());
                return false;
            }

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
            m_IndexBuffer = std::make_unique<IndexBufferData>();
            m_IndexBuffer->indexType = indexData->GetIndexType();

            if (indexData->GetIndexType() == IndexType::UInt16)
            {
                m_IndexBuffer->buffer16 = std::make_unique<IndexBuffer<uint16_t>>();
                const uint16_t* indexPtr = static_cast<const uint16_t*>(indexData->GetData());

                if (!m_IndexBuffer->buffer16->Initialize(indexPtr, static_cast<UINT>(indexData->GetIndexCount()), UsageType::Immutable))
                {
                    OutputDebugStringA("Failed to create uint16 index buffer\n");
                    m_IndexBuffer.reset();
                    return false;
                }
            }
            else
            {
                m_IndexBuffer->buffer32 = std::make_unique<IndexBuffer<uint32_t>>();
                const uint32_t* indexPtr = static_cast<const uint32_t*>(indexData->GetData());

                if (!m_IndexBuffer->buffer32->Initialize(indexPtr, static_cast<UINT>(indexData->GetIndexCount()), UsageType::Immutable))
                {
                    OutputDebugStringA("Failed to create uint32 index buffer\n");
                    m_IndexBuffer.reset();
                    return false;
                }
            }

            m_IndexCount = indexData->GetIndexCount();
            m_IndexType = indexData->GetIndexType();
        }

        m_Topology = static_cast<PrimitiveTopology>(resource.GetTopology());
        return true;
    }

    bool MeshBuffers::CreateFromVertexData(const VertexData& vertexData, const IndexData* indexData)
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

        return CreateFromResource(tempResource);
    }

    bool MeshBuffers::AddVertexBuffer(const VertexData& vertexData, uint32_t slot)
    {
        const void* data = vertexData.GetVertexData(slot);
        size_t dataSize = vertexData.GetDataSize(slot);
        uint32_t stride = vertexData.GetLayout().GetStride(slot);

        if (dataSize == 0)
            return false;

        BufferDesc bufferDesc;
        bufferDesc.bufferType = BufferType::Vertex;
        bufferDesc.usageType = UsageType::Immutable;  // Default for static mesh data
        bufferDesc.byteWidth = static_cast<UINT>(dataSize);
        bufferDesc.initialData = data;

        auto vertexBuffer = std::make_unique<RawBuffer>();

        if (!vertexBuffer->Initialize(bufferDesc))
        {
            OutputDebugStringA(("Failed to create vertex buffer for slot " + std::to_string(slot) + "\n").c_str());
            return false;
        }

        VertexBufferData vbData;
        vbData.buffer = std::move(vertexBuffer);
        vbData.stride = stride;
        vbData.offset = 0;

        m_VertexBuffers[slot] = std::move(vbData);
        return true;
    }

    void MeshBuffers::Bind(uint32_t startSlot) const
    {
        BindVertexBuffers(startSlot);
        if (m_IndexBuffer)
            BindIndexBuffer();
    }

    void MeshBuffers::BindVertexBuffers(uint32_t startSlot) const
    {
        // Find the range of slots we need to bind
        if (m_VertexBuffers.empty())
            return;

        uint32_t maxSlot = 0;
        for (const auto& [slot, data] : m_VertexBuffers)
        {
            maxSlot = std::max(maxSlot, slot);
        }

        // Create arrays for binding
        std::vector<ID3D11Buffer*> buffers(maxSlot + 1, nullptr);
        std::vector<UINT> strides(maxSlot + 1, 0);
        std::vector<UINT> offsets(maxSlot + 1, 0);

        for (const auto& [slot, data] : m_VertexBuffers)
        {
            buffers[slot] = data.buffer->GetBuffer();
            strides[slot] = data.stride;
            offsets[slot] = data.offset;
        }

        RenderCommand::GetContext()->IASetVertexBuffers(
            startSlot,
            static_cast<UINT>(buffers.size()),
            buffers.data(),
            strides.data(),
            offsets.data()
        );
    }

    void MeshBuffers::BindIndexBuffer() const
    {
        if (!m_IndexBuffer)
            return;
        RenderCommand::GetContext()->IASetIndexBuffer(m_IndexBuffer->GetBuffer(), m_IndexBuffer->GetFormat(), 0);
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

    size_t MeshBuffers::GetGPUMemoryUsage() const
    {
        size_t usage = 0;

        for (const auto& [slot, data] : m_VertexBuffers)
        {
            usage += m_VertexCount * data.stride;
        }

        if (m_IndexBuffer && m_IndexCount > 0)
        {
            size_t indexSize = m_IndexType == IndexType::UInt32 ? sizeof(uint32_t) : sizeof(uint16_t);
            usage += m_IndexCount * indexSize;
        }

        return usage;
    }
}