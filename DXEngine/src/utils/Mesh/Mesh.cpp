#include "dxpch.h"
#include "Mesh.h"
#include "renderer/RendererCommand.h"
#include <utils/material/Material.h>
#include <utils/IndexBuffer.h>
#include <cassert>
#include <sstream>
#include <algorithm>

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

            // Create D3D11 vertex buffer
            D3D11_BUFFER_DESC bufferDesc = {};
            bufferDesc.Usage = D3D11_USAGE_DEFAULT;
            bufferDesc.ByteWidth = static_cast<UINT>(dataSize);
            bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = data;

            Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
            HRESULT hr = RenderCommand::GetDevice()->CreateBuffer(&bufferDesc, &initData, &buffer);
            if (FAILED(hr))
            {
                OutputDebugStringA(("Failed to create vertex buffer for slot " + std::to_string(attr.Slot) + "\n").c_str());
                return false;
            }

            VertexBufferData vbData;
            vbData.buffer = buffer;
            vbData.stride = stride;
            vbData.offset = 0;

            m_VertexBuffers[attr.Slot] = vbData;
        }

        m_VertexCount = vertexData->GetVertexCount();

        // Create index buffer if available
        if (indexData && indexData->GetIndexCount() > 0)
        {
            DXGI_FORMAT indexFormat = indexData->GetIndexType() == IndexType::UInt32 ?
                DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

            D3D11_BUFFER_DESC bufferDesc = {};
            bufferDesc.Usage = D3D11_USAGE_DEFAULT;
            bufferDesc.ByteWidth = static_cast<UINT>(indexData->GetDataSize());
            bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = indexData->GetData();

            HRESULT hr = RenderCommand::GetDevice()->CreateBuffer(&bufferDesc, &initData, &m_IndexBuffer);
            if (FAILED(hr))
            {
                OutputDebugStringA("Failed to create index buffer\n");
                return false;
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

        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = static_cast<UINT>(dataSize);
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = data;

        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        HRESULT hr = RenderCommand::GetDevice()->CreateBuffer(&bufferDesc, &initData, &buffer);
        if (FAILED(hr))
            return false;

        VertexBufferData vbData;
        vbData.buffer = buffer;
        vbData.stride = stride;
        vbData.offset = 0;

        m_VertexBuffers[slot] = vbData;
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
            buffers[slot] = data.buffer.Get();
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

        DXGI_FORMAT format = m_IndexType == IndexType::UInt32 ?
            DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

        RenderCommand::GetContext()->IASetIndexBuffer(m_IndexBuffer.Get(), format, 0);
    }

    void MeshBuffers::Release()
    {
        m_VertexBuffers.clear();
        m_IndexBuffer.Reset();
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

    // ===== InputLayoutCache Implementation =====

    InputLayoutCache& InputLayoutCache::Instance()
    {
        static InputLayoutCache instance;
        return instance;
    }

    Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayoutCache::GetInputLayout(
        const VertexLayout& vertexLayout,
        const void* shaderByteCode,
        size_t byteCodeLength)
    {
        // Create cache key
        LayoutKey key;
        key.vertexLayoutHash = std::hash<std::string>{}(vertexLayout.GetDebugString());
        key.shaderHash = std::hash<std::string>{}(std::string((const char*)shaderByteCode, byteCodeLength));

        // Check cache
        auto it = m_Cache.find(key);
        if (it != m_Cache.end())
            return it->second;

        // Create new input layout
        auto elements = vertexLayout.CreateD3D11InputElements();

        Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
        HRESULT hr = RenderCommand::GetDevice()->CreateInputLayout(
            elements.data(),
            static_cast<UINT>(elements.size()),
            shaderByteCode,
            byteCodeLength,
            &inputLayout
        );

        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create input layout\n");
            return nullptr;
        }

        // Cache and return
        m_Cache[key] = inputLayout;
        return inputLayout;
    }

    void InputLayoutCache::ClearCache()
    {
        m_Cache.clear();
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

        // Update GPU buffer
        if (!matrices.empty())
        {
            D3D11_BUFFER_DESC desc = {};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth = static_cast<UINT>(matrices.size() * sizeof(DirectX::XMFLOAT4X4));
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = matrices.data();

            RenderCommand::GetDevice()->CreateBuffer(&desc, &initData, &m_BoneBuffer);
        }
    }

    void SkinnedMesh::BindBoneData() const
    {
        if (m_BoneBuffer)
        {
            RenderCommand::GetContext()->VSSetConstantBuffers(1, 1, m_BoneBuffer.GetAddressOf());
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

        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = static_cast<UINT>(dataSize);
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = data;

        HRESULT hr = RenderCommand::GetDevice()->CreateBuffer(&desc, &initData, &m_InstanceBuffer);
        if (FAILED(hr))
            return false;

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

        void ComputeNormals(VertexData& vertices, const IndexData& indices)
        {
            const VertexLayout& layout = vertices.GetLayout();
            if (!layout.HasAttribute(VertexAttributeType::Position) ||
                !layout.HasAttribute(VertexAttributeType::Normal))
            {
                OutputDebugStringA("MeshUtils::ComputeNormals - Missing required attributes\n");
                return;
            }

            // Zero out existing normals
            DirectX::XMFLOAT3 zeroNormal(0.0f, 0.0f, 0.0f);
            for (size_t i = 0; i < vertices.GetVertexCount(); ++i)
            {
                vertices.SetAttribute(i, VertexAttributeType::Normal, zeroNormal);
            }

            // Calculate face normals and accumulate
            size_t indexCount = indices.GetIndexCount();
            for (size_t i = 0; i < indexCount; i += 3)
            {
                uint32_t i0 = indices.GetIndex(i);
                uint32_t i1 = indices.GetIndex(i + 1);
                uint32_t i2 = indices.GetIndex(i + 2);

                auto p0 = vertices.GetAttribute<DirectX::XMFLOAT3>(i0, VertexAttributeType::Position);
                auto p1 = vertices.GetAttribute<DirectX::XMFLOAT3>(i1, VertexAttributeType::Position);
                auto p2 = vertices.GetAttribute<DirectX::XMFLOAT3>(i2, VertexAttributeType::Position);

                // Calculate face normal
                DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&p0);
                DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&p1);
                DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&p2);

                DirectX::XMVECTOR edge1 = DirectX::XMVectorSubtract(v1, v0);
                DirectX::XMVECTOR edge2 = DirectX::XMVectorSubtract(v2, v0);
                DirectX::XMVECTOR normal = DirectX::XMVector3Cross(edge1, edge2);

                DirectX::XMFLOAT3 faceNormal;
                DirectX::XMStoreFloat3(&faceNormal, normal);

                // Accumulate to vertex normals
                auto n0 = vertices.GetAttribute<DirectX::XMFLOAT3>(i0, VertexAttributeType::Normal);
                auto n1 = vertices.GetAttribute<DirectX::XMFLOAT3>(i1, VertexAttributeType::Normal);
                auto n2 = vertices.GetAttribute<DirectX::XMFLOAT3>(i2, VertexAttributeType::Normal);

                n0.x += faceNormal.x; n0.y += faceNormal.y; n0.z += faceNormal.z;
                n1.x += faceNormal.x; n1.y += faceNormal.y; n1.z += faceNormal.z;
                n2.x += faceNormal.x; n2.y += faceNormal.y; n2.z += faceNormal.z;

                vertices.SetAttribute(i0, VertexAttributeType::Normal, n0);
                vertices.SetAttribute(i1, VertexAttributeType::Normal, n1);
                vertices.SetAttribute(i2, VertexAttributeType::Normal, n2);
            }

            // Normalize all normals
            for (size_t i = 0; i < vertices.GetVertexCount(); ++i)
            {
                auto normal = vertices.GetAttribute<DirectX::XMFLOAT3>(i, VertexAttributeType::Normal);
                DirectX::XMVECTOR normalVec = DirectX::XMLoadFloat3(&normal);
                normalVec = DirectX::XMVector3Normalize(normalVec);
                DirectX::XMStoreFloat3(&normal, normalVec);
                vertices.SetAttribute(i, VertexAttributeType::Normal, normal);
            }
        }

        void ComputeTangents(VertexData& vertices, const IndexData& indices)
        {
            const VertexLayout& layout = vertices.GetLayout();
            if (!layout.HasAttribute(VertexAttributeType::Position) ||
                !layout.HasAttribute(VertexAttributeType::Normal) ||
                !layout.HasAttribute(VertexAttributeType::Tangent) ||
                !layout.HasAttribute(VertexAttributeType::TexCoord0))
            {
                OutputDebugStringA("MeshUtils::ComputeTangents - Missing required attributes\n");
                return;
            }

            size_t vertexCount = vertices.GetVertexCount();

            // Initialize tangent vectors
            std::vector<DirectX::XMFLOAT3> tangents(vertexCount, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));
            std::vector<DirectX::XMFLOAT3> bitangents(vertexCount, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

            // Calculate tangents using Lengyel's method
            size_t indexCount = indices.GetIndexCount();
            for (size_t i = 0; i < indexCount; i += 3)
            {
                uint32_t i0 = indices.GetIndex(i);
                uint32_t i1 = indices.GetIndex(i + 1);
                uint32_t i2 = indices.GetIndex(i + 2);

                auto p0 = vertices.GetAttribute<DirectX::XMFLOAT3>(i0, VertexAttributeType::Position);
                auto p1 = vertices.GetAttribute<DirectX::XMFLOAT3>(i1, VertexAttributeType::Position);
                auto p2 = vertices.GetAttribute<DirectX::XMFLOAT3>(i2, VertexAttributeType::Position);

                auto uv0 = vertices.GetAttribute<DirectX::XMFLOAT2>(i0, VertexAttributeType::TexCoord0);
                auto uv1 = vertices.GetAttribute<DirectX::XMFLOAT2>(i1, VertexAttributeType::TexCoord0);
                auto uv2 = vertices.GetAttribute<DirectX::XMFLOAT2>(i2, VertexAttributeType::TexCoord0);

                DirectX::XMFLOAT3 edge1(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
                DirectX::XMFLOAT3 edge2(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z);

                DirectX::XMFLOAT2 deltaUV1(uv1.x - uv0.x, uv1.y - uv0.y);
                DirectX::XMFLOAT2 deltaUV2(uv2.x - uv0.x, uv2.y - uv0.y);

                float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

                DirectX::XMFLOAT3 tangent(
                    f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
                    f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
                    f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)
                );

                DirectX::XMFLOAT3 bitangent(
                    f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x),
                    f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y),
                    f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z)
                );

                // Accumulate tangents
                tangents[i0].x += tangent.x; tangents[i0].y += tangent.y; tangents[i0].z += tangent.z;
                tangents[i1].x += tangent.x; tangents[i1].y += tangent.y; tangents[i1].z += tangent.z;
                tangents[i2].x += tangent.x; tangents[i2].y += tangent.y; tangents[i2].z += tangent.z;

                bitangents[i0].x += bitangent.x; bitangents[i0].y += bitangent.y; bitangents[i0].z += bitangent.z;
                bitangents[i1].x += bitangent.x; bitangents[i1].y += bitangent.y; bitangents[i1].z += bitangent.z;
                bitangents[i2].x += bitangent.x; bitangents[i2].y += bitangent.y; bitangents[i2].z += bitangent.z;
            }

            // Orthogonalize and normalize tangents
            for (size_t i = 0; i < vertexCount; ++i)
            {
                auto normal = vertices.GetAttribute<DirectX::XMFLOAT3>(i, VertexAttributeType::Normal);

                DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&normal);
                DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&tangents[i]);
                DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&bitangents[i]);

                // Gram-Schmidt orthogonalize
                t = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(t, DirectX::XMVectorMultiply(n, DirectX::XMVector3Dot(n, t))));

                // Calculate handedness
                float handedness = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Cross(n, t), b)) < 0.0f ? -1.0f : 1.0f;

                DirectX::XMFLOAT4 finalTangent;
                DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&finalTangent), t);
                finalTangent.w = handedness;

                vertices.SetAttribute(i, VertexAttributeType::Tangent, finalTangent);
            }
        }

        void ComputeBounds(const VertexData& vertices, BoundingBox& box, BoundingSphere& sphere)
        {
            const VertexLayout& layout = vertices.GetLayout();
            if (!layout.HasAttribute(VertexAttributeType::Position) || vertices.GetVertexCount() == 0)
            {
                box = BoundingBox();
                sphere = BoundingSphere();
                return;
            }

            // Initialize bounds with first vertex
            auto firstPos = vertices.GetAttribute<DirectX::XMFLOAT3>(0, VertexAttributeType::Position);
            box = BoundingBox(firstPos, firstPos);

            // Expand bounds with all vertices
            for (size_t i = 1; i < vertices.GetVertexCount(); ++i)
            {
                auto pos = vertices.GetAttribute<DirectX::XMFLOAT3>(i, VertexAttributeType::Position);
                box.Expand(pos);
            }

            // Compute bounding sphere
            DirectX::XMFLOAT3 center = box.GetCenter();
            float maxRadiusSquared = 0.0f;

            for (size_t i = 0; i < vertices.GetVertexCount(); ++i)
            {
                auto pos = vertices.GetAttribute<DirectX::XMFLOAT3>(i, VertexAttributeType::Position);
                float dx = pos.x - center.x;
                float dy = pos.y - center.y;
                float dz = pos.z - center.z;
                float radiusSquared = dx * dx + dy * dy + dz * dz;
                maxRadiusSquared = std::max(maxRadiusSquared, radiusSquared);
            }

            sphere = BoundingSphere(center, sqrtf(maxRadiusSquared));
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