#pragma once
#include "utils/Buffer.h"
#include "utils/Mesh/Utils/IndexData.h"
#include "utils/Mesh/Resource/MeshResource.h"

namespace DXEngine
{
    class VertexData;

    class MeshBuffers
    {
    public:
        MeshBuffers() = default;
        ~MeshBuffers();

        // Non-copyable but movable
        MeshBuffers(const MeshBuffers&) = delete;
        MeshBuffers& operator=(const MeshBuffers&) = delete;
        MeshBuffers(MeshBuffers&&) = default;
        MeshBuffers& operator=(MeshBuffers&&) = default;

        // Buffer creation from mesh resource
        bool CreateFromResource(const MeshResource& resource);
        bool CreateFromVertexData(const VertexData& vertexData, const IndexData* indexData = nullptr);

        // Multiple vertex buffer support for complex meshes
        bool AddVertexBuffer(const VertexData& vertexData, uint32_t slot);

        // GPU resource access
        void Bind(uint32_t startSlot = 0) const;
        void BindVertexBuffers(uint32_t startSlot = 0) const;
        void BindIndexBuffer() const;

        // Resource management
        void Release();
        bool IsValid() const;

        // Properties
        size_t GetVertexCount() const { return m_VertexCount; }
        size_t GetIndexCount() const { return m_IndexCount; }
        IndexType GetIndexType() const { return m_IndexType; }
        PrimitiveTopology GetTopology() const { return m_Topology; }

        // Memory usage
        size_t GetGPUMemoryUsage() const;

    private:
        struct VertexBufferData
        {
            std::unique_ptr<RawBuffer> buffer;
            uint32_t stride;
            uint32_t offset;
        };
        struct IndexBufferData
        {
            std::unique_ptr<IndexBuffer<uint16_t>> buffer16;
            std::unique_ptr<IndexBuffer<uint32_t>> buffer32;
            IndexType indexType;

            ID3D11Buffer* GetBuffer() const
            {
                return indexType == IndexType::UInt16 ?
                    (buffer16 ? buffer16->GetBuffer() : nullptr) :
                    (buffer32 ? buffer32->GetBuffer() : nullptr);
            }

            DXGI_FORMAT GetFormat() const
            {
                return indexType == IndexType::UInt16 ?
                    DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
            }

            bool IsValid() const
            {
                return indexType == IndexType::UInt16 ?
                    (buffer16 && buffer16->IsValid()) :
                    (buffer32 && buffer32->IsValid());
            }
        };

        std::unordered_map<uint32_t, VertexBufferData> m_VertexBuffers;
        std::unique_ptr<IndexBufferData> m_IndexBuffer;

        size_t m_VertexCount = 0;
        size_t m_IndexCount = 0;
        IndexType m_IndexType = IndexType::UInt16;
        PrimitiveTopology m_Topology = PrimitiveTopology::TriangleList;
    };

}

