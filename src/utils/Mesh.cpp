#include "Mesh.h"
#include <algorithm>


namespace DXEngine {

	void MeshResource::ComputeBounds()
	{
		if (m_Vertices.empty())return;
		auto& v0 = m_Vertices[0].pos;
		DirectX::XMFLOAT3 min = v0, max = v0;
		for (auto& v : m_Vertices)
		{
			min.x = std::min(min.x, v.pos.x);
			min.y = std::min(min.y, v.pos.y);
			min.z = std::min(min.z, v.pos.z);
			max.x = std::max(max.x, v.pos.x);
			max.y = std::max(max.y, v.pos.y);
			max.z = std::max(max.z, v.pos.z);
		}

		m_AABB = { min,max };

		//sphere: center = (min, max)/2 radius = max distance 
		m_Sphere.center = { (min.x + max.x) / 2, (min.y + max.y) / 2, (min.z + max.z) / 2 };

		float r2 = 0;
		for (auto& v : m_Vertices)
		{
			float dx = v.pos.x - m_Sphere.center.x;
			float dy = v.pos.y - m_Sphere.center.y;
			float dz = v.pos.z - m_Sphere.center.z;
			r2 = std::max(r2, dx * dx + dy * dy * dz + dz);
		}

		m_Sphere.radius = std::sqrt(r2);
	}

	Mesh::Mesh( const std::shared_ptr<MeshResource>& resource)
		:
		m_Resource(resource),
		m_IndexCount(resource->GetIndices().size()),
		tp(, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
		m_MeshMaterial(nullptr)

	{
		m_VertexBuffer = std::make_shared<VertexBuffer>(, resource->GetVertices());
		m_IndexBuffer = std::make_shared<IndexBuffer>(, resource->GetIndices());
		tp.Bind();
	}

	Mesh::~Mesh()
	{

	}

	void Mesh::SetMaterial(const std::shared_ptr<Material>& material)
	{
		m_MeshMaterial = material;

	}

	void Mesh::Draw()
	{
		if (m_MeshMaterial)
			m_MeshMaterial->Bind();

		m_VertexBuffer->Bind();
		m_IndexBuffer->Bind();
		//psBuffer.data.light.spotPos.x = XMVectorGetX(camPos);
		//psBuffer.data.light.spotPos.y = XMVectorGetY(camPos);
		//psBuffer.data.light.spotPos.z = XMVectorGetZ(camPos);
		//
		////light dir
		//psBuffer.data.light.dir.x = XMVectorGetX(camTarget) - psBuffer.data.light.spotPos.x;
		//psBuffer.data.light.dir.y = XMVectorGetY(camTarget) - psBuffer.data.light.spotPos.y;
		//psBuffer.data.light.dir.z = XMVectorGetZ(camTarget) - psBuffer.data.light.spotPos.z;
		RenderCommand:: Draw(m_IndexCount);
	}

	const std::shared_ptr<MeshResource>& Mesh::GetResource() const
	{
		return m_Resource;
	}
}


