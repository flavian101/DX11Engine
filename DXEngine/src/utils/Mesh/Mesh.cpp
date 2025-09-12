#include "dxpch.h"
#include "Mesh.h"
#include "renderer/RendererCommand.h"
#include <utils/material/Material.h>
#include <cassert>
#include <sstream>
#include <algorithm>
#include "InputManager.h"


namespace DXEngine {

    // ===== MeshBuffers Implementation =====

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

            m_VertexBuffers[attr.Slot] =std::move(vbData);
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

        if(!vertexBuffer->Initialize(bufferDesc))
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

    // ===== Mesh Implementation =====

    Mesh::Mesh(std::shared_ptr<MeshResource> resource)
        : m_Resource(resource)
        , m_GPUResourcesDirty(true)
    {
        if (m_Resource)
        {
            EnsureMaterialSlots();
        }
    }

    void Mesh::SetResource(std::shared_ptr<MeshResource> resource)
    {
        if (m_Resource != resource)
        {
            m_Resource = resource;
            InvalidateGPUResources();
            EnsureMaterialSlots();
            OnResourceChanged();
        }
    }

    bool Mesh::EnsureGPUResources() const
    {
        if (!m_GPUResourcesDirty && m_Buffers.IsValid())
            return true;

        if (!m_Resource || !m_Resource->IsValid())
            return false;

        bool success = m_Buffers.CreateFromResource(*m_Resource);
        if (success)
        {
            m_GPUResourcesDirty = false;
        }

        return success;
    }

    void Mesh::ReleaseGPUResources()
    {
        m_Buffers.Release();
        m_GPUResourcesDirty = true;
    }

    void Mesh::SetMaterial(std::shared_ptr<Material> material)
    {
        SetMaterial(0, material);
    }

    void Mesh::SetMaterial(size_t submeshIndex, std::shared_ptr<Material> material)
    {
        EnsureMaterialSlots();

        if (submeshIndex >= m_Materials.size())
            return;

        if (m_Materials[submeshIndex] != material)
        {
            m_Materials[submeshIndex] = material;
            OnMaterialChanged(submeshIndex);
        }
    }

    const std::shared_ptr<Material>& Mesh::GetMaterial(size_t submeshIndex) const
    {
        static std::shared_ptr<Material> nullMaterial;

        if (submeshIndex >= m_Materials.size())
            return nullMaterial;

        return m_Materials[submeshIndex];
    }

    void Mesh::Bind(const void* shaderByteCode, size_t byteCodeLength) const
    {
        if (!EnsureGPUResources())
            return;

        // Bind vertex buffers and index buffer
        m_Buffers.Bind();

        // Set up input layout if shader bytecode is provided
        if (shaderByteCode && byteCodeLength > 0 && m_Resource)
        {
            const VertexData* vertexData = m_Resource->GetVertexData();
            if (vertexData)
            {
                auto inputLayout = InputLayoutCache::Instance().GetInputLayout(
                    vertexData->GetLayout(), shaderByteCode, byteCodeLength);

                if (inputLayout)
                {
                    RenderCommand::GetContext()->IASetInputLayout(inputLayout.Get());
                }
            }
        }

        // Set primitive topology
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        switch (m_Buffers.GetTopology())
        {
        case PrimitiveTopology::TriangleList: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
        case PrimitiveTopology::TriangleStrip: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
        case PrimitiveTopology::LineList: topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;
        case PrimitiveTopology::LineStrip: topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
        case PrimitiveTopology::PointList: topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;
        }
        RenderCommand::GetContext()->IASetPrimitiveTopology(topology);
    }

    void Mesh::Draw(size_t submeshIndex) const
    {
        if (!EnsureGPUResources() || !m_Resource)
            return;

        if (m_Resource->HasSubmeshes())
        {
            if (submeshIndex >= m_Resource->GetSubMeshCount())
                return;

            const auto& submesh = m_Resource->GetSubMesh(submeshIndex);

            if (m_Buffers.GetIndexCount() > 0)
            {
                RenderCommand::GetContext()->DrawIndexed(
                    submesh.indexCount,
                    submesh.indexStart,
                    submesh.vertexStart
                );
            }
            else
            {
                RenderCommand::GetContext()->Draw(
                    submesh.vertexCount,
                    submesh.vertexStart
                );
            }
        }
        else
        {
            // Draw entire mesh
            if (m_Buffers.GetIndexCount() > 0)
            {
                RenderCommand::GetContext()->DrawIndexed(
                    static_cast<UINT>(m_Buffers.GetIndexCount()),
                    0,
                    0
                );
            }
            else
            {
                RenderCommand::GetContext()->Draw(
                    static_cast<UINT>(m_Buffers.GetVertexCount()),
                    0
                );
            }
        }
    }

    void Mesh::DrawAll() const
    {
        if (!m_Resource)
            return;

        if (m_Resource->HasSubmeshes())
        {
            for (size_t i = 0; i < m_Resource->GetSubMeshCount(); ++i)
            {
                // Bind material for this submesh if available
                if (i < m_Materials.size() && m_Materials[i])
                {
                    m_Materials[i]->Bind();
                }

                Draw(i);
            }
        }
        else
        {
            // Bind default material
            if (!m_Materials.empty() && m_Materials[0])
            {
                m_Materials[0]->Bind();
            }

            Draw(0);
        }
    }

    void Mesh::DrawInstanced(uint32_t instanceCount, size_t submeshIndex) const
    {
        if (!EnsureGPUResources() || !m_Resource || instanceCount == 0)
            return;

        if (m_Resource->HasSubmeshes())
        {
            if (submeshIndex >= m_Resource->GetSubMeshCount())
                return;

            const auto& submesh = m_Resource->GetSubMesh(submeshIndex);

            if (m_Buffers.GetIndexCount() > 0)
            {
                RenderCommand::GetContext()->DrawIndexedInstanced(
                    submesh.indexCount,
                    instanceCount,
                    submesh.indexStart,
                    submesh.vertexStart,
                    0
                );
            }
            else
            {
                RenderCommand::GetContext()->DrawInstanced(
                    submesh.vertexCount,
                    instanceCount,
                    submesh.vertexStart,
                    0
                );
            }
        }
        else
        {
            if (m_Buffers.GetIndexCount() > 0)
            {
                RenderCommand::GetContext()->DrawIndexedInstanced(
                    static_cast<UINT>(m_Buffers.GetIndexCount()),
                    instanceCount,
                    0,
                    0,
                    0
                );
            }
            else
            {
                RenderCommand::GetContext()->DrawInstanced(
                    static_cast<UINT>(m_Buffers.GetVertexCount()),
                    instanceCount,
                    0,
                    0
                );
            }
        }
    }

    bool Mesh::IsValid() const
    {
        return m_Resource && m_Resource->IsValid();
    }

    size_t Mesh::GetSubmeshCount() const
    {
        return m_Resource ? m_Resource->GetSubMeshCount() : 0;
    }

    size_t Mesh::GetVertexCount()const
    {
        return m_Resource ? m_Resource->GetVertexData()->GetVertexCount() : 0;
    }
    size_t Mesh::GetIndexCount()const
    {
        return m_Resource ? m_Resource->GetIndexData()->GetIndexCount() : 0;
    }

    bool Mesh::HasMaterial(size_t submeshIndex) const
    {
        return submeshIndex < m_Materials.size() && m_Materials[submeshIndex] != nullptr;
    }

    const BoundingBox& Mesh::GetBoundingBox() const
    {
        static BoundingBox emptyBox;
        return m_Resource ? m_Resource->GetBoundingBox() : emptyBox;
    }

    const BoundingSphere& Mesh::GetBoundingSphere() const
    {
        static BoundingSphere emptySphere;
        return m_Resource ? m_Resource->GetBoundingSphere() : emptySphere;
    }

    std::string Mesh::GetDebugInfo() const
    {
        std::ostringstream oss;
        oss << "Mesh Debug Info:\n";

        if (m_Resource)
        {
            oss << m_Resource->GetDebugInfo();
        }
        else
        {
            oss << "No resource\n";
        }

        oss << "GPU Resources: " << (m_Buffers.IsValid() ? "Valid" : "Invalid") << "\n";
        oss << "Memory Usage: " << GetTotalMemoryUsage() << " bytes\n";
        oss << "Materials: " << m_Materials.size() << "\n";

        return oss.str();
    }

    size_t Mesh::GetTotalMemoryUsage() const
    {
        size_t usage = 0;

        if (m_Resource)
        {
            usage += m_Resource->GetMemoryUsage();
        }

        if (m_Buffers.IsValid())
        {
            usage += m_Buffers.GetGPUMemoryUsage();
        }

        return usage;
    }

    void Mesh::OnResourceChanged()
    {
        InvalidateGPUResources();
        EnsureMaterialSlots();
    }

    void Mesh::OnMaterialChanged(size_t submeshIndex)
    {
        // Override in derived classes if needed
    }

    void Mesh::InvalidateGPUResources()
    {
        m_GPUResourcesDirty = true;
    }

    void Mesh::EnsureMaterialSlots()
    {
        if (!m_Resource)
        {
            m_Materials.clear();
            return;
        }

        size_t requiredSlots = std::max(size_t(1), m_Resource->GetSubMeshCount());
        if (m_Materials.size() != requiredSlots)
        {
            m_Materials.resize(requiredSlots);
        }
    }

    // ===== Factory Methods =====

    std::shared_ptr<Mesh> Mesh::CreateQuad(float width, float height)
    {
        auto resource = MeshResource::CreateQuad("Quad", width, height);
        return std::make_shared<Mesh>(std::move(resource));
    }

    std::shared_ptr<Mesh> Mesh::CreateCube(float size)
    {
        auto resource = MeshResource::CreateCube("Cube", size);
        return std::make_shared<Mesh>(std::move(resource));
    }

    std::shared_ptr<Mesh> Mesh::CreateSphere(float radius, uint32_t segments)
    {
        auto resource = MeshResource::CreateSphere("Sphere", radius, segments);
        return std::make_shared<Mesh>(std::move(resource));
    }

    std::shared_ptr<Mesh> Mesh::CreatePlane(float width, float depth, uint32_t widthSegments, uint32_t depthSegments)
    {
        auto resource = MeshResource::CreatePlane("Plane", width, depth, widthSegments, depthSegments);
        return std::make_shared<Mesh>(std::move(resource));
    }

    // ===== SkinnedMesh Implementation =====

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

    // ===== InstancedMesh Implementation =====

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

    // ===== MeshUtils Implementation =====

    namespace MeshUtils
    {
        std::shared_ptr<MeshResource> GenerateQuad(float width, float height, bool generateNormals, bool generateTangents)
        {
            auto resource = MeshResource::CreateQuad("GeneratedQuad", width, height);
            if (generateTangents && !generateNormals) {
                resource->GenerateNormals(); // Tangents require normals
            }
            if (generateTangents) {
                resource->GenerateTangents();
            }
            return resource;
        }

        std::shared_ptr<MeshResource> GenerateCube(float size, bool generateNormals, bool generateTangents)
        {
            auto resource = MeshResource::CreateCube("GeneratedCube", size);
            if (generateTangents && !generateNormals) {
                resource->GenerateNormals();
            }
            if (generateTangents) {
                resource->GenerateTangents();
            }
            return resource;
        }

        std::shared_ptr<MeshResource> GenerateSphere(float radius, uint32_t rings, uint32_t segments, bool generateNormals, bool generateTangents)
        {
            auto resource = MeshResource::CreateSphere("GeneratedSphere", radius, segments);
            if (generateTangents && !generateNormals) {
                resource->GenerateNormals();
            }
            if (generateTangents) {
                resource->GenerateTangents();
            }
            return resource;
        }

        std::shared_ptr<MeshResource> GenerateCylinder(float radius, float height, uint32_t segments, uint32_t rings, bool generateNormals, bool generateTangents)
        {
            auto resource = MeshResource::CreateCylinder("GeneratedCylinder", radius, height, segments);
            if (generateTangents && !generateNormals) {
                resource->GenerateNormals();
            }
            if (generateTangents) {
                resource->GenerateTangents();
            }
            return resource;
        }

        std::shared_ptr<MeshResource> GeneratePlane(float width, float depth, uint32_t widthSegments, uint32_t depthSegments, bool generateNormals, bool generateTangents)
        {
            auto resource = MeshResource::CreatePlane("GeneratedPlane", width, depth, widthSegments, depthSegments);
            if (generateTangents && !generateNormals) {
                resource->GenerateNormals();
            }
            if (generateTangents) {
                resource->GenerateTangents();
            }
            return resource;
        }

        void OptimizeVertexCache(IndexData& indices)
        {
            indices.OptimizeForCache();
        }

        void OptimizeVertexFetch(VertexData& vertices, IndexData& indices)
        {
            // This is a complex optimization that would typically use external libraries
            // like meshoptimizer. For now, we'll just do basic vertex cache optimization
            OptimizeVertexCache(indices);
            OutputDebugStringA("MeshUtils::OptimizeVertexFetch - Basic optimization applied\n");
        }

        bool ValidateMesh(const MeshResource& resource, std::string& errorMessage)
        {
            if (!resource.IsValid())
            {
                errorMessage = "Mesh resource is invalid";
                return false;
            }

            const VertexData* vertexData = resource.GetVertexData();
            const IndexData* indexData = resource.GetIndexData();

            if (!vertexData)
            {
                errorMessage = "No vertex data";
                return false;
            }

            if (vertexData->GetVertexCount() == 0)
            {
                errorMessage = "No vertices";
                return false;
            }

            if (!vertexData->IsValid())
            {
                errorMessage = "Invalid vertex data";
                return false;
            }

            // Check vertex layout has position
            const VertexLayout& layout = vertexData->GetLayout();
            if (!layout.HasAttribute(VertexAttributeType::Position))
            {
                errorMessage = "Vertex layout missing position attribute";
                return false;
            }

            // Validate submeshes if present
            if (resource.HasSubmeshes())
            {
                for (size_t i = 0; i < resource.GetSubMeshCount(); ++i)
                {
                    const auto& submesh = resource.GetSubMesh(i);

                    if (indexData)
                    {
                        if (submesh.indexStart + submesh.indexCount > indexData->GetIndexCount())
                        {
                            errorMessage = "Submesh " + std::to_string(i) + " indices out of range";
                            return false;
                        }
                    }

                    if (submesh.vertexStart + submesh.vertexCount > vertexData->GetVertexCount())
                    {
                        errorMessage = "Submesh " + std::to_string(i) + " vertices out of range";
                        return false;
                    }
                }
            }

            errorMessage = "Mesh is valid";
            return true;
        }
    }

}