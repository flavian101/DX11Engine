#pragma once
#include "Mesh.h"
#include "Sphere.h"
#include <wrl.h>
#include "CubeMapTexture.h"
#include <memory>



class SkySphere
{
	
public:
	SkySphere(Graphics& g);

	void Draw(Graphics& g, FXMVECTOR camPos);
	Mesh getSky(Graphics& g);
	CubeMapTexture getSkyTexture(Graphics& g);

private:
	std::unique_ptr<Sphere> sphere;
	XMMATRIX skyPos;
	DirectX::XMMATRIX Scale;
	DirectX::XMMATRIX Translation;

};

