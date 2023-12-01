#pragma once
#include "Graphics.h"
#include "Mesh.h"




class Triangle
{
private:
	Vertex v[4] =
	{
		Vertex(-0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),
		Vertex(-0.5f,  0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f),
		Vertex(0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f),
		Vertex(0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f),
	};


	const unsigned short indices[6] =
	{
		 0, 1, 2,
		 0, 2, 3,
	};

public:
	Triangle(Graphics& g);
	void Draw(Graphics& g);
private:

	Mesh tria;
};

