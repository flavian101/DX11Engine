#pragma once
#define NOMINMAX
#include "VertexAttribute.h"
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <string>
#include <variant>

namespace DXEngine
{
	enum class IndexType
	{
		UInt16,
		UInt32
	};
	enum class PrimitiveTopology
	{
		TriangleList,
		TriangleStrip,
		LineList,
		LineStrip,
		PointList,
		TriangleListAdj,   
		TriangleStripAdj,
		LineListAdj,
		LineStripAdj
	};
	struct BoundingBox
	{
		DirectX::XMFLOAT3 min;
		DirectX::XMFLOAT3 max;

		BoundingBox() : min(FLT_MAX, FLT_MAX, FLT_MAX), max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}
		BoundingBox(const DirectX::XMFLOAT3& minPoint, const DirectX::XMFLOAT3& maxPoint)
			: min(minPoint), max(maxPoint) {
		}

		DirectX::XMFLOAT3 GetCenter() const;
		DirectX::XMFLOAT3 GetExtents() const;
		void GetCorners(DirectX::XMVECTOR corners[8]) const;
		float GetRadius() const;
		void Expand(const DirectX::XMFLOAT3& point);
		void Expand(const BoundingBox& other);
		bool Contains(const DirectX::XMFLOAT3& point) const;
		bool Intersects(const BoundingBox& other) const;
	};


	struct BoundingSphere
	{
		DirectX::XMFLOAT3 center;
		float radius;

		BoundingSphere() : center(0.0f, 0.0f, 0.0f), radius(0.0f) {}
		BoundingSphere(const DirectX::XMFLOAT3& c, float r) : center(c), radius(r) {}

		bool Contains(const DirectX::XMFLOAT3& point) const;
		bool Intersects(const BoundingSphere& other) const;
		bool Intersects(const BoundingBox& box) const;
	};

	struct SubMesh
	{
		std::string name;
		uint32_t indexStart;
		uint32_t indexCount;
		uint32_t vertexStart;    // For when using single vertex buffer
		uint32_t vertexCount;
		uint32_t materialIndex;  // Index into material array
		BoundingBox bounds;

		SubMesh() : indexStart(0), indexCount(0), vertexStart(0), vertexCount(0), materialIndex(0) {}
		SubMesh(const std::string& n, uint32_t idxStart, uint32_t idxCount,
			uint32_t vtxStart = 0, uint32_t vtxCount = 0, uint32_t matIdx = 0)
			: name(n), indexStart(idxStart), indexCount(idxCount)
			, vertexStart(vtxStart), vertexCount(vtxCount), materialIndex(matIdx) {
		}
	};

	//index Buffer Container
	class IndexData
	{
	public:
		IndexData(IndexType type = IndexType::UInt16) : m_IndexType(type) {}

		void SetIndexType(IndexType type);
		IndexType GetIndexType() const { return m_IndexType; }

		void Reserve(size_t count);
		void Clear();

		// Add indices
		void AddIndex(uint32_t index);
		void AddTriangle(uint32_t i0, uint32_t i1, uint32_t i2);
		void AddQuad(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3); // Two triangles

		// Bulk operations
		template<typename T>
		void SetIndices(const std::vector<T>& indices);

		void SetIndices(const std::vector<uint16_t>& indices);
		void SetIndices(const std::vector<uint32_t>& indices);

		// Access
		size_t GetIndexCount() const;
		size_t GetDataSize() const;
		const void* GetData() const;
		void* GetData();

		// Utility
		uint32_t GetIndex(size_t index) const;
		void SetIndex(size_t index, uint32_t value);

		// Optimization
		void OptimizeForCache();  // Vertex cache optimization
		void GenerateAdjacency(const VertexData& vertices, std::vector<uint32_t>& adjacency);

	private:
		IndexType m_IndexType;
		std::variant<std::vector<uint16_t>, std::vector<uint32_t>> m_Indices;
	};

	class MeshResource
	{
	public:
		MeshResource() = default;
		explicit MeshResource(const std::string& name) : m_Name(name) {}
		virtual ~MeshResource() = default;

		// Basic properties
		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		// Vertex data management
		void SetVertexData(std::unique_ptr<VertexData> vertexData);
		const VertexData* GetVertexData() const { return m_VertexData.get(); }
		VertexData* GetVertexData() { return m_VertexData.get(); }

		// Index data management
		void SetIndexData(std::unique_ptr<IndexData> indexData);
		const IndexData* GetIndexData() const { return m_IndexData.get(); }
		IndexData* GetIndexData() { return m_IndexData.get(); }

		// Topology
		void SetTopology(PrimitiveTopology topology) { m_Topology = topology; }
		PrimitiveTopology GetTopology() const { return m_Topology; }

		// Submesh management
		void AddSubMesh(const SubMesh& submesh);
		void AddSubMesh(const std::string& name, uint32_t indexStart, uint32_t indexCount,
			uint32_t materialIndex = 0);
		const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }
		SubMesh& GetSubMesh(size_t index) { return m_SubMeshes[index]; }
		const SubMesh& GetSubMesh(size_t index) const { return m_SubMeshes[index]; }
		size_t GetSubMeshCount() const { return m_SubMeshes.size(); }

		// Bounding volumes
		const BoundingBox& GetBoundingBox() const { return m_BoundingBox; }
		const BoundingSphere& GetBoundingSphere() const { return m_BoundingSphere; }
		void ComputeBounds();
		void SetBounds(const BoundingBox& box, const BoundingSphere& sphere);

		// Utility methods
		bool IsValid() const;
		bool HasIndices() const { return m_IndexData != nullptr && m_IndexData->GetIndexCount() > 0; }
		bool HasSubmeshes() const { return !m_SubMeshes.empty(); }

		// Mesh generation helpers
		void GenerateNormals();
		void GenerateTangents();
		void GenerateBounds();

		// Optimization
		void OptimizeForRendering();

		// Debug/Statistics
		size_t GetMemoryUsage() const;
		std::string GetDebugInfo() const;

		// Factory methods for common shapes
		static std::unique_ptr<MeshResource> CreateQuad(const std::string& name = "Quad",
			float width = 1.0f, float height = 1.0f);
		static std::unique_ptr<MeshResource> CreateCube(const std::string& name = "Cube",
			float size = 1.0f);
		static std::unique_ptr<MeshResource> CreateSphere(const std::string& name = "Sphere",
			float radius = 1.0f, uint32_t segments = 32);
		static std::unique_ptr<MeshResource> CreateCylinder(const std::string& name = "Cylinder",
			float radius = 1.0f, float height = 2.0f,
			uint32_t segments = 32);
		static std::unique_ptr<MeshResource> CreatePlane(const std::string& name = "Plane",
			float width = 10.0f, float depth = 10.0f,
			uint32_t widthSegments = 10, uint32_t depthSegments = 10);

	protected:
		virtual void OnBoundsChanged() {}
		virtual void OnDataChanged() {}

	private:
		void InvalidateBounds();

	private:
		std::string m_Name;
		std::unique_ptr<VertexData> m_VertexData;
		std::unique_ptr<IndexData> m_IndexData;
		PrimitiveTopology m_Topology = PrimitiveTopology::TriangleList;

		std::vector<SubMesh> m_SubMeshes;

		BoundingBox m_BoundingBox;
		BoundingSphere m_BoundingSphere;
		mutable bool m_BoundsDirty = true;

	};

	// Specialized mesh resource for specific use cases
	class SkinnedMeshResource : public MeshResource
	{
	public:
		struct BoneInfo
		{
			std::string name;
			DirectX::XMFLOAT4X4 offsetMatrix;
			int32_t parentIndex = -1;
		};

		SkinnedMeshResource(const std::string& name = "SkinnedMesh") : MeshResource(name) {}

		// Bone management
		void AddBone(const BoneInfo& bone);
		const std::vector<BoneInfo>& GetBones() const { return m_Bones; }
		BoneInfo& GetBone(size_t index) { return m_Bones[index]; }
		const BoneInfo& GetBone(size_t index) const { return m_Bones[index]; }
		size_t GetBoneCount() const { return m_Bones.size(); }

		int32_t FindBoneIndex(const std::string& name) const;

	private:
		std::vector<BoneInfo> m_Bones;
	};

	// Instanced mesh resource for rendering many similar objects
	class InstancedMeshResource : public MeshResource
	{
	public:
		InstancedMeshResource(const std::string& name = "InstancedMesh") : MeshResource(name) {}

		// Instance data management
		void SetInstanceLayout(const VertexLayout& layout);
		const VertexLayout& GetInstanceLayout() const { return m_InstanceLayout; }

		void SetInstanceData(std::unique_ptr<VertexData> instanceData);
		const VertexData* GetInstanceData() const { return m_InstanceData.get(); }
		VertexData* GetInstanceData() { return m_InstanceData.get(); }

		size_t GetInstanceCount() const;

	private:
		VertexLayout m_InstanceLayout;
		std::unique_ptr<VertexData> m_InstanceData;
	};
	template<typename T>
	inline void IndexData::SetIndices(const std::vector<T>& indices)
	{
	}
	inline size_t IndexData::GetDataSize() const
	{
		return size_t();
	}
	inline const void* IndexData::GetData() const
	{
		return nullptr;
	}
	inline void* IndexData::GetData()
	{
		return nullptr;
	}
	inline uint32_t IndexData::GetIndex(size_t index) const
	{
		return 0;
	}
	inline void IndexData::SetIndex(size_t index, uint32_t value)
	{
	}
}
