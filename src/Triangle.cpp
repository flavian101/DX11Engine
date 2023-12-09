#include "Triangle.h"


Triangle::Triangle(Graphics& g)
	//tria(g, indices, v,(sizeof(indices) / sizeof(indices[0])), sizeof(v)),
	//tex(g,"assets/textures/grass.jpg",0)
{
	vertices.push_back( Vertex(-1.0f, -1.0f, -1.0f, 100.0f, 100.0f, 0.0f, 1.0f, 0.0f));
	vertices.push_back(Vertex(1.0f, -1.0f, -1.0f, 0.0f, 100.0f, 0.0f, 1.0f, 0.0f));
	vertices.push_back(Vertex(1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	vertices.push_back(Vertex(-1.0f, -1.0f, 1.0f, 100.0f, 0.0f, 0.0f, 1.0f, 0.0f));

	ind.push_back(0);
	ind.push_back(1);
	ind.push_back(2);
	ind.push_back(0);
	ind.push_back(2);
	ind.push_back(3);

	texCache = std::make_shared<TextureCache>(g);

	auto texture = texCache.get()->GetTexture("assets/textures/grass.jpg", 1);

	g.GetContext()->PSSetShaderResources(0, 1, texture.GetAddressOf());

	


}

void Triangle::Draw(Graphics& g, FXMVECTOR camPos, FXMVECTOR camTarget)
{
	squareMatrix = XMMatrixIdentity();

	Scale = XMMatrixScaling(500.0f, 10.0f, 500.0f);
	Translation = XMMatrixTranslation(0.0f, 10.0f, 0.0f);


	squareMatrix = Scale * Translation;

	//tex.Bind(g);
	getGround(g).Draw(g, squareMatrix, camPos, camTarget);
}

Mesh Triangle::getGround(Graphics& g)
{
	return Mesh(g, ind, vertices,
		L"assets/shaders/vs.cso",
		L"assets/shaders/ps.cso"
	);
}


