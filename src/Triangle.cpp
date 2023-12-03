#include "Triangle.h"


Triangle::Triangle(Graphics& g)
	:
	tria(g, indices, v,(sizeof(indices) / sizeof(indices[0])), sizeof(v)),
	tex(g,"assets/textures/grass.jpg")
{

	
	
}

void Triangle::Draw(Graphics& g, XMVECTOR camPos, XMVECTOR camTarget)
{
	squareMatrix = XMMatrixIdentity();
	
	Scale = XMMatrixScaling(500.0f, 10.0f, 500.0f);
	Translation = XMMatrixTranslation(0.0f, 10.0f, 0.0f);

	
	squareMatrix = Scale * Translation;
	tex.Bind(g);
	tria.Draw(g,squareMatrix,camPos, camTarget);
}


