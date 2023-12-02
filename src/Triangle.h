#pragma once
#include "Graphics.h"
#include "Mesh.h"
#include <DirectXMath.h>
#include "Texture.h"




class Triangle
{
private:
	 Vertex v[24] =
	{
		 // Front Face
			Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 1.0f),
			Vertex(-1.0f,  1.0f, -1.0f, 0.0f, 0.0f),
			Vertex(1.0f,  1.0f, -1.0f, 1.0f, 0.0f),
			Vertex(1.0f, -1.0f, -1.0f, 1.0f, 1.0f),

			// Back Face
			Vertex(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f),
			Vertex(1.0f, -1.0f, 1.0f, 0.0f, 1.0f),
			Vertex(1.0f,  1.0f, 1.0f, 0.0f, 0.0f),
			Vertex(-1.0f,  1.0f, 1.0f, 1.0f, 0.0f),

			// Top Face
			Vertex(-1.0f, 1.0f, -1.0f, 0.0f, 1.0f),
			Vertex(-1.0f, 1.0f,  1.0f, 0.0f, 0.0f),
			Vertex(1.0f, 1.0f,  1.0f, 1.0f, 0.0f),
			Vertex(1.0f, 1.0f, -1.0f, 1.0f, 1.0f),

			// Bottom Face
			Vertex(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f),
			Vertex(1.0f, -1.0f, -1.0f, 0.0f, 1.0f),
			Vertex(1.0f, -1.0f,  1.0f, 0.0f, 0.0f),
			Vertex(-1.0f, -1.0f,  1.0f, 1.0f, 0.0f),

			// Left Face
			Vertex(-1.0f, -1.0f,  1.0f, 0.0f, 1.0f),
			Vertex(-1.0f,  1.0f,  1.0f, 0.0f, 0.0f),
			Vertex(-1.0f,  1.0f, -1.0f, 1.0f, 0.0f),
			Vertex(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f),

			// Right Face
			Vertex(1.0f, -1.0f, -1.0f, 0.0f, 1.0f),
			Vertex(1.0f,  1.0f, -1.0f, 0.0f, 0.0f),
			Vertex(1.0f,  1.0f,  1.0f, 1.0f, 0.0f),
			Vertex(1.0f, -1.0f,  1.0f, 1.0f, 1.0f),
	};


	const unsigned short indices[36] =
	{
		// Front Face
		  0,  1,  2,
		  0,  2,  3,

		  // Back Face
		  4,  5,  6,
		  4,  6,  7,

		  // Top Face
		  8,  9, 10,
		  8, 10, 11,

		  // Bottom Face
		  12, 13, 14,
		  12, 14, 15,

		  // Left Face
		  16, 17, 18,
		  16, 18, 19,

		  // Right Face
		  20, 21, 22,
		  20, 22, 23
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
	Texture tex;

};

