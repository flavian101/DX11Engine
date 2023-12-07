#include "Triangle.h"


Triangle::Triangle(Graphics& g)
	:
	//tria(g, indices, v,(sizeof(indices) / sizeof(indices[0])), sizeof(v)),
	tex(g,"assets/textures/grass.jpg",0)
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
	


}

void Triangle::Draw(Graphics& g, XMVECTOR camPos, XMVECTOR camTarget)
{
	squareMatrix = XMMatrixIdentity();

	Scale = XMMatrixScaling(500.0f, 10.0f, 500.0f);
	Translation = XMMatrixTranslation(0.0f, 10.0f, 0.0f);


	squareMatrix = Scale * Translation;

	tex.Bind(g);
	getGround(g).Draw(g, squareMatrix, camPos, camTarget);
}

Mesh Triangle::getGround(Graphics& g)
{
	return Mesh(g, ind, vertices,
		L"assets/shaders/vs.cso",
		L"assets/shaders/ps.cso"
	);
}


