#pragma once
#include "RHI/GraphicsDevice.h"
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

        // Buffer creation from mesh resource (Device Must outlive MeshBuffer we use Shared_Ptr)
        bool CreateFromResource(RHI::IGraphicsDevice* device, const MeshResource& resource);
        bool CreateFromVertexData(RHI::IGraphicsDevice* device,const VertexData& vertexData, const IndexData* indexData = nullptr, RHI::PrimitiveTopology topology);

        // Multiple vertex buffer support for complex meshes
        bool AddVertexBuffer(RHI::IGraphicsDevice* device, const VertexData& vertexData, uint32_t slot);

        // GPU resource access
        void Bind(RHI::ICommandBuffer* cmd,uint32_t startSlot = 0) const;

        // Resource management
        void Release();
        bool IsValid() const;

        // Properties
        size_t GetVertexCount() const { return m_VertexCount; }
        size_t GetIndexCount() const { return m_IndexCount; }
        IndexType GetIndexType() const { return m_IndexType; }
        RHI::PrimitiveTopology GetTopology() const { return m_Topology; }

        // Memory usage
        size_t GetGPUMemoryUsage() const;
    private:
        //create index Buffer
        bool CreateIndexBuffer(RHI::IGraphicsDevice* device, const IndexData* indexData, const std::string& debugName);
        //bind index and vertex buffers
        void BindVertexBuffers(RHI::ICommandBuffer* cmd, uint32_t startSlot = 0) const;
        void BindIndexBuffer(RHI::ICommandBuffer* cmd) const;
    private:
        struct VertexBufferData
        {
            std::shared_ptr<RHI::IBuffer> buffer;
            uint32_t stride;
            uint32_t offset;
        };
        struct IndexBufferInfo
        {
            std::shared_ptr<RHI::IBuffer> buffer;  // Single buffer
            IndexType indexType;

            bool IsValid() const
            {
                return buffer && buffer->IsValid();
            }
        };

        std::unordered_map<uint32_t, VertexBufferData> m_VertexBuffers;
        std::unique_ptr<IndexBufferInfo> m_IndexBuffer;

        size_t m_VertexCount = 0;
        size_t m_IndexCount = 0;
        IndexType m_IndexType = IndexType::UInt16;
        RHI::PrimitiveTopology m_Topology = RHI::PrimitiveTopology::TriangleList;
    };

}

