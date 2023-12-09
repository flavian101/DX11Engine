#pragma once
#include "Mesh.h"
#include "Sphere.h"
#include "Graphics.h"
#include "Texture.h"
#include <memory>


class Ball
{
public:
	Ball(Graphics& g);

	void Draw(Graphics& g, XMVECTOR camPos, XMVECTOR camTarget);
	void SetPos(XMMATRIX trans);
	Mesh getBal(Graphics& g);


private:
	Sphere sphere;
	XMMATRIX ballPos;
	//Texture tex;
	std::unique_ptr<TextureCache> texCache;
	DirectX::XMMATRIX Scale;
	DirectX::XMMATRIX Translation;


};

