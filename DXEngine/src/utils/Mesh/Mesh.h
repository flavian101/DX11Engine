#pragma once
#include "MeshResource.h"
#include "renderer/RendererCommand.h"
#include <memory>
#include <unordered_map>
#include "IndexData.h"



namespace DXEngine {
	class Material;
	class VertexBuffer;
	class IndexBuffer;
	class InputLayout;

    // GPU resource management for vertex/index buffers
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
            Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
            uint32_t stride;
            uint32_t offset;
        };

        std::unordered_map<uint32_t, VertexBufferData> m_VertexBuffers;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndexBuffer;

        size_t m_VertexCount = 0;
        size_t m_IndexCount = 0;
        IndexType m_IndexType = IndexType::UInt16;
        PrimitiveTopology m_Topology = PrimitiveTopology::TriangleList;
    };

    // Input layout cache for shader compatibility
    class InputLayoutCache
    {
    public:
        static InputLayoutCache& Instance();

        // Get or create input layout for vertex layout + shader combination
        Microsoft::WRL::ComPtr<ID3D11InputLayout> GetInputLayout(
            const VertexLayout& vertexLayout,
            const void* shaderByteCode,
            size_t byteCodeLength);

        void ClearCache();

    private:
        InputLayoutCache() = default;

        struct LayoutKey
        {
            size_t vertexLayoutHash;
            size_t shaderHash;

            bool operator==(const LayoutKey& other) const
            {
                return vertexLayoutHash == other.vertexLayoutHash && shaderHash == other.shaderHash;
            }
        };

        struct LayoutKeyHasher
        {
            size_t operator()(const LayoutKey& key) const
            {
                return key.vertexLayoutHash ^ (key.shaderHash << 1);
            }
        };

        std::unordered_map<LayoutKey, Microsoft::WRL::ComPtr<ID3D11InputLayout>, LayoutKeyHasher> m_Cache;
    };

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

    // Specialized mesh types
    class SkinnedMesh : public Mesh
    {
    public:
        explicit SkinnedMesh(std::shared_ptr<SkinnedMeshResource> resource);

        const std::shared_ptr<SkinnedMeshResource>& GetSkinnedResource() const
        {
            return std::static_pointer_cast<SkinnedMeshResource>(GetResource());
        }

        // Bone matrices for animation
        void SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices);
        const std::vector<DirectX::XMFLOAT4X4>& GetBoneMatrices() const { return m_BoneMatrices; }

        // Bind bone data for rendering
        void BindBoneData() const;

    private:
        std::vector<DirectX::XMFLOAT4X4> m_BoneMatrices;
        mutable Microsoft::WRL::ComPtr<ID3D11Buffer> m_BoneBuffer;
    };

    class InstancedMesh : public Mesh
    {
    public:
        explicit InstancedMesh(std::shared_ptr<InstancedMeshResource> resource);

        const std::shared_ptr<InstancedMeshResource>& GetInstancedResource() const
        {
            return std::static_pointer_cast<InstancedMeshResource>(GetResource());
        }

        // Instance rendering
        void DrawInstanced(size_t submeshIndex = 0) const;
        void DrawAllInstanced() const;

        // Update instance data on GPU
        bool UpdateInstanceData() const;

    protected:
        void OnResourceChanged() override;

    private:
        mutable Microsoft::WRL::ComPtr<ID3D11Buffer> m_InstanceBuffer;
        mutable bool m_InstanceDataDirty = true;
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

