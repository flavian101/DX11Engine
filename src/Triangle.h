#pragma once
#include "Graphics.h"
#include "Mesh.h"
#include <DirectXMath.h>




class Triangle
{
private:
	 Vertex v[8] =
	{
		Vertex(-1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f),
		Vertex(-1.0f, +1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f),
		Vertex(+1.0f, +1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
		Vertex(+1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f),
		Vertex(-1.0f, -1.0f, +1.0f, 0.0f, 1.0f, 1.0f, 1.0f),
		Vertex(-1.0f, +1.0f, +1.0f, 1.0f, 1.0f, 1.0f, 1.0f),
		Vertex(+1.0f, +1.0f, +1.0f, 1.0f, 0.0f, 1.0f, 1.0f),
		Vertex(+1.0f, -1.0f, +1.0f, 1.0f, 0.0f, 0.0f, 1.0f),
	};


	const unsigned short indices[36] =
	{
		// front face
	   0, 1, 2,
	   0, 2, 3,

	   // back face
	   4, 6, 5,
	   4, 7, 6,

	   // left face
	   4, 5, 1,
	   4, 1, 0,

	   // right face
	   3, 2, 6,
	   3, 6, 7,

	   // top face
	   1, 5, 6,
	   1, 6, 2,

	   // bottom face
	   4, 0, 3,
	   4, 3, 7
	};

public:
	Triangle(Graphics& g);
	void Draw(Graphics& g);
private:
	DirectX::XMMATRIX squareMatrix;
	DirectX::XMMATRIX squareMatrix2;
	DirectX::XMMATRIX Rotation;
	DirectX::XMMATRIX Scale;
	DirectX::XMMATRIX Translation;
	float rot = 0.01f;
	Mesh tria;
};

