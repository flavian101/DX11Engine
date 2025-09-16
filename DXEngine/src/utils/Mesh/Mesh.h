#pragma once
#include "Resource/MeshResource.h"
#include "renderer/RendererCommand.h"
#include <memory>
#include <unordered_map>
#include "utils/Mesh/Utils/IndexData.h"
#include "utils/Mesh/Utils/MeshBuffers.h"
#include "utils/Buffer.h"


namespace DXEngine {
	class Material;
	class InputLayout;
      
    // Main Mesh class - represents a renderable mesh
    class Mesh
    {
    public:
        explicit Mesh(std::shared_ptr<MeshResource> resource);
        virtual ~Mesh() = default;

        // Resource management
        const std::shared_ptr<MeshResource>& GetResource() const { return m_Resource; }
        void SetResource(std::shared_ptr<MeshResource> resource);

        // GPU resources
        bool EnsureGPUResources()const;
        void ReleaseGPUResources();
        const MeshBuffers& GetBuffers() const { return m_Buffers; }

        // Material management
        void SetMaterial(std::shared_ptr<Material> material);
        void SetMaterial(size_t submeshIndex, std::shared_ptr<Material> material);
        const std::shared_ptr<Material>& GetMaterial(size_t submeshIndex = 0) const;
        const std::vector<std::shared_ptr<Material>>& GetMaterials() const { return m_Materials; }

        // Rendering
        void Bind(const void* shaderByteCode = nullptr, size_t byteCodeLength = 0) const;
        void Draw(size_t submeshIndex = 0) const;
        void DrawAll() const;  // Draw all submeshes
        void DrawInstanced(uint32_t instanceCount, size_t submeshIndex = 0) const;

        // Properties
        bool IsValid() const;
        size_t GetSubmeshCount() const;
        size_t GetVertexCount()const;
        size_t GetIndexCount()const;
        bool HasMaterial(size_t submeshIndex = 0) const;

        // Bounding information
        const BoundingBox& GetBoundingBox() const;
        const BoundingSphere& GetBoundingSphere() const;

        // Debug
        std::string GetDebugInfo() const;
        size_t GetTotalMemoryUsage() const;

        // Factory methods
        static std::shared_ptr<Mesh> CreateQuad(float width = 1.0f, float height = 1.0f);
        static std::shared_ptr<Mesh> CreateCube(float size = 1.0f);
        static std::shared_ptr<Mesh> CreateSphere(float radius = 1.0f, uint32_t segments = 32);
        static std::shared_ptr<Mesh> CreatePlane(float width = 10.0f, float depth = 10.0f,
            uint32_t widthSegments = 10, uint32_t depthSegments = 10);

    protected:
        virtual void OnResourceChanged();
        virtual void OnMaterialChanged(size_t submeshIndex);

    private:
        void InvalidateGPUResources();
        void EnsureMaterialSlots();

    private:
        std::shared_ptr<MeshResource> m_Resource;
        mutable MeshBuffers m_Buffers;
        std::vector<std::shared_ptr<Material>> m_Materials;
        mutable bool m_GPUResourcesDirty = true;
    };

   
    // Mesh loading / creation utilities
        namespace MeshUtils
        {
            // Generate common mesh shapes
            std::shared_ptr<MeshResource> GenerateQuad(float width = 1.0f, float height = 1.0f,
                bool generateNormals = true, bool generateTangents = true);

            std::shared_ptr<MeshResource> GenerateCube(float size = 1.0f,
                bool generateNormals = true, bool generateTangents = true);

            std::shared_ptr<MeshResource> GenerateSphere(float radius = 1.0f, uint32_t rings = 32, uint32_t segments = 32,
                bool generateNormals = true, bool generateTangents = true);

            std::shared_ptr<MeshResource> GenerateCylinder(float radius = 1.0f, float height = 2.0f,
                uint32_t segments = 32, uint32_t rings = 1,
                bool generateNormals = true, bool generateTangents = true);

            std::shared_ptr<MeshResource> GeneratePlane(float width = 10.0f, float depth = 10.0f,
                uint32_t widthSegments = 10, uint32_t depthSegments = 10,
                bool generateNormals = true, bool generateTangents = true);

            // Mesh processing utilities
            void ComputeNormals(VertexData& vertices, const IndexData& indices);
            void ComputeTangents(VertexData& vertices, const IndexData& indices);
            void ComputeBounds(const VertexData& vertices, BoundingBox& box, BoundingSphere& sphere);

            // Mesh optimization
            void OptimizeVertexCache(IndexData& indices);
            void OptimizeVertexFetch(VertexData& vertices, IndexData& indices);

            // Validation
            bool ValidateMesh(const MeshResource& resource, std::string& errorMessage);
        }
}

