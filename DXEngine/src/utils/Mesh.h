#pragma once
#include "Vertex.h"
#include "renderer/RendererCommand.h"
#include "VertexBuffer.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "InputLayout.h"
#include "Topology.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "Sampler.h"
#include "material/Material.h"
#include <memory>



namespace DXEngine {

	// Bounding volumes (stubs)
	struct BoundingBox { DirectX::XMFLOAT3 minimum, maximum; };
	struct BoundingSphere { DirectX::XMFLOAT3 center; float radius; };
	;

	class MeshResource
	{
	public:
		using VertexList = std::vector<Vertex>;
		using IndexList = std::vector<unsigned short>;


		MeshResource(VertexList&& verts, IndexList&& idxs)
			:
			m_Vertices(std::move(verts)),
			m_Indices(std::move(idxs))
		{
			ComputeBounds();
		}

		const VertexList& GetVertices() const { return m_Vertices; }
		const IndexList& GetIndices()  const { return m_Indices; }
		const BoundingBox& GetAABB()     const { return m_AABB; }
		const BoundingSphere& GetSphere()  const { return m_Sphere; }

	private:
		void ComputeBounds();


	private:
		VertexList      m_Vertices;
		IndexList       m_Indices;
		BoundingBox     m_AABB;
		BoundingSphere  m_Sphere;
	};


	class Mesh
	{
	public:
		Mesh(const std::shared_ptr<MeshResource>& resource);
		~Mesh();

		void SetMaterial(const std::shared_ptr<Material>& material);
		const std::shared_ptr<Material>& GetMaterial() const { return m_MeshMaterial; }
		uint32_t GetIndexCount() const { return m_IndexCount; }
		const Topology& GetTopology() const { return tp; }

		void BindBuffers();
		const std::shared_ptr<MeshResource>& GetResource() const;
	private:
		std::shared_ptr<MeshResource> m_Resource;
		Topology tp;
		UINT m_IndexCount;
		std::shared_ptr<VertexBuffer> m_VertexBuffer;
		std::shared_ptr<IndexBuffer> m_IndexBuffer;
		std::shared_ptr<Material> m_MeshMaterial;
	public:


	};
}

