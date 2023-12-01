#include "Triangle.h"


Triangle::Triangle(Graphics& g)
	:
	tria(g, indices, v,(sizeof(indices) / sizeof(indices[0])))
{

	squareMatrix = XMMatrixIdentity();
}

void Triangle::Draw(Graphics& g)
{
	tria.Draw(g,squareMatrix);
}


