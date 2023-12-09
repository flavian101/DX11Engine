#pragma once
#include "Graphics.h"
#include "Mesh.h"
#include <DirectXMath.h>
#include "Texture.h"




class Triangle
{
private:

public:
	Triangle(Graphics& g);
	void Draw(Graphics& g,FXMVECTOR camPos, FXMVECTOR camTarget);
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
	//Texture tex;
	std::shared_ptr<TextureCache> texCache;


};

