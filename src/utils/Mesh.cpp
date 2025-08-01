#include "Mesh.h"

void MeshResource::ComputeBounds()
{
	if (m_Vertices.empty())return;
	auto& v0 = m_Vertices[0].pos;
	XMFLOAT3 min = v0, max = v0;
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

Mesh::Mesh(Graphics& gfx, const std::shared_ptr<MeshResource>& resource)
	:
	m_IndexCount(resource->GetIndices().size()),
	tp(gfx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
	m_MeshMaterial(nullptr)

{
	m_VertexBuffer = std::make_shared<VertexBuffer>(gfx, resource->GetVertices());
	m_IndexBuffer = std::make_shared<IndexBuffer>(gfx, resource->GetIndices());
	tp.Bind(gfx);
}

Mesh::~Mesh()
{

}

void Mesh::SetMaterial(const std::shared_ptr<Material>& material)
{
	m_MeshMaterial = material;

}

void Mesh::Draw(Graphics& gfx)
{
	if(m_MeshMaterial)
		m_MeshMaterial->Bind(gfx);

	m_VertexBuffer->Bind(gfx);
	m_IndexBuffer->Bind(gfx);
	//psBuffer.data.light.spotPos.x = XMVectorGetX(camPos);
	//psBuffer.data.light.spotPos.y = XMVectorGetY(camPos);
	//psBuffer.data.light.spotPos.z = XMVectorGetZ(camPos);
	//
	////light dir
	//psBuffer.data.light.dir.x = XMVectorGetX(camTarget) - psBuffer.data.light.spotPos.x;
	//psBuffer.data.light.dir.y = XMVectorGetY(camTarget) - psBuffer.data.light.spotPos.y;
	//psBuffer.data.light.dir.z = XMVectorGetZ(camTarget) - psBuffer.data.light.spotPos.z;
	gfx.Draw(m_IndexCount);
}



