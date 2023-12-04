#pragma once
#include "Graphics.h"
#include "Mesh.h"
#include <DirectXMath.h>
#include "Texture.h"




class Triangle
{
private:

	
	
//	Vertex v[4] =
//	{
//		// Bottom Face
//	   Vertex(-1.0f, -1.0f, -1.0f, 100.0f, 100.0f, 0.0f, 1.0f, 0.0f),
//	   Vertex(1.0f, -1.0f, -1.0f,   0.0f, 100.0f, 0.0f, 1.0f, 0.0f),
//	   Vertex(1.0f, -1.0f,  1.0f,   0.0f,   0.0f, 0.0f, 1.0f, 0.0f),
//	   Vertex(-1.0f, -1.0f,  1.0f, 100.0f,   0.0f, 0.0f, 1.0f, 0.0f),
//	};
//	const unsigned short indices[6] =
//	{
//		 0,  1,  2,
//		 0,  2,  3,
//	};

	


public:
	Triangle(Graphics& g);
	void Draw(Graphics& g,XMVECTOR camPos, XMVECTOR camTarget);
	Mesh getGround(Graphics& g);
private:
	std::vector<Vertex> vertices;
	std::vector<unsigned short> ind;
	DirectX::XMMATRIX squareMatrix;
	DirectX::XMMATRIX squareMatrix2;
	DirectX::XMMATRIX Rotation;
	DirectX::XMMATRIX Scale;
	DirectX::XMMATRIX Translation;
	float rot = 0.01f;
	//Mesh tria;
	Texture tex;

};

