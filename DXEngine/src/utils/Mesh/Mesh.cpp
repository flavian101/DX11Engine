#include "dxpch.h"
#include "Mesh.h"
#include "renderer/RendererCommand.h"
#include <utils/material/Material.h>
#include <cassert>
#include <sstream>
#include <algorithm>
#include "utils/Mesh/Utils/InputManager.h"


namespace DXEngine {

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