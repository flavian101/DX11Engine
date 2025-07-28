#include "Triangle.h"


Triangle::Triangle(Graphics& g)
	//tria(g, indices, v,(sizeof(indices) / sizeof(indices[0])), sizeof(v)),
{
	XMFLOAT4 tangent(1.0f, 0.0f, 0.0f,1.0f);
	
	vertices.push_back(Vertex(-1.0f, -1.0f, -1.0f, 100.0f, 100.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));
	vertices.push_back(Vertex( 1.0f, -1.0f, -1.0f,   0.0f, 100.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));
	vertices.push_back(Vertex( 1.0f, -1.0f,  1.0f,   0.0f,   0.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));
	vertices.push_back(Vertex(-1.0f, -1.0f,  1.0f, 100.0f,   0.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));


	ind.push_back(0);
	ind.push_back(1);
	ind.push_back(2);
	ind.push_back(0);
	ind.push_back(2);
	ind.push_back(3);

	Initialize(g);
	auto grassMaterial = std::make_shared<Material>(g);
	auto grassDiffuse = std::make_shared<Texture>(g, "assets/textures/grass.jpg");
	auto grassNormal = std::make_shared<Texture>(g, "assets/textures/grassNormal.jpg");
	grassMaterial->SetDiffuse(grassDiffuse);
	grassMaterial->SetNormalMap(grassNormal);
	groundMesh->SetMaterial(grassMaterial);


}
void Triangle::Initialize(Graphics& g)
{
	groundMesh = std::make_unique<Mesh>(g, ind, vertices,
		L"assets/shaders/vs.cso",
		L"assets/shaders/ps.cso");

}
void Triangle::Draw(Graphics& g, FXMVECTOR camPos, FXMVECTOR camTarget)
{
	squareMatrix = XMMatrixIdentity();

	Scale = XMMatrixScaling(500.0f, 10.0f, 500.0f);
	Translation = XMMatrixTranslation(0.0f, 10.0f, 0.0f);


	squareMatrix = Scale * Translation;

	groundMesh->Draw(g, squareMatrix, camPos, camTarget);
}





