#include "Triangle.h"


Triangle::Triangle(Graphics& gfx, std::shared_ptr<ShaderProgram> program)
	:
	Model(gfx, program)
{
	Initialize(gfx);
	auto grassMaterial = std::make_shared<Material>(gfx);
	auto grassDiffuse = std::make_shared<Texture>(gfx, "assets/textures/grass.jpg");
	auto grassNormal = std::make_shared<Texture>(gfx, "assets/textures/grassNormal.jpg");
	grassMaterial->SetShaderProgram(program);
	grassMaterial->SetDiffuse(grassDiffuse);
	grassMaterial->SetNormalMap(grassNormal);
	m_Mesh->SetMaterial(grassMaterial);
}
void Triangle::Initialize(Graphics& gfx)
{
	std::vector<Vertex> vertices;
	std::vector<unsigned short> ind;

	XMFLOAT4 tangent(1.0f, 0.0f, 0.0f, 1.0f);

	vertices.push_back(Vertex(-1.0f, -1.0f, -1.0f, 100.0f, 100.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));
	vertices.push_back(Vertex(1.0f, -1.0f, -1.0f, 0.0f, 100.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));
	vertices.push_back(Vertex(1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));
	vertices.push_back(Vertex(-1.0f, -1.0f, 1.0f, 100.0f, 0.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));


	ind.push_back(0);
	ind.push_back(1);
	ind.push_back(2);
	ind.push_back(0);
	ind.push_back(2);
	ind.push_back(3);

	auto meshData = std::make_shared<MeshResource>(std::move(vertices), std::move(ind));
	m_Mesh = std::make_shared<Mesh>(gfx,meshData);

}
void Triangle::Render(Graphics& gfx)
{
	Model::Render(gfx);
}





