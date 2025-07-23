#pragma once
#include "Mesh.h"
#include "Sphere.h"
#include "Graphics.h"
#include "Texture.h"


class Ball
{
public:
	Ball(Graphics& g);

	void Draw(Graphics& g, XMVECTOR camPos, XMVECTOR camTarget);
	void SetPos(XMMATRIX trans);
	Mesh getBal(Graphics& g);
	void Initialize(Graphics& g);


private:
	Sphere sphere;
	std::unique_ptr<Mesh> ballMesh;
	XMMATRIX ballPos;
	Texture tex;
	DirectX::XMMATRIX Scale;
	DirectX::XMMATRIX Translation;


};

