#include "Triangle.h"


Triangle::Triangle(Graphics& g)
	:
	tria(g, indices, v,(sizeof(indices) / sizeof(indices[0])), sizeof(v)),
	tex(g,"assets/textures/icon.png")
{

	squareMatrix = XMMatrixIdentity();
	squareMatrix2 = XMMatrixIdentity();
}

void Triangle::Draw(Graphics& g)
{
	//Keep the cubes rotating
	rot += .02f;
	if (rot > 6.26f)
		rot = 0.0f;
	//Define cube1's world space matrix
	XMVECTOR rotaxis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	Rotation = XMMatrixRotationAxis(rotaxis, rot);
	Translation = XMMatrixTranslation(0.0f, 0.0f, 4.0f);

	//Set cube1's world space using the transformations
	squareMatrix = Translation * Rotation;

	//Define cube2's world space matrix
	Rotation = XMMatrixRotationAxis(rotaxis, -rot);
	Scale = XMMatrixScaling(1.3f, 1.3f, 1.3f);

	//Set cube2's world space matrix
	squareMatrix2 = Rotation * Scale;
	tex.Bind(g);
	tria.Draw(g,squareMatrix);
	tria.Draw(g, squareMatrix2);
}


