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