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
	Mesh* getSky(Graphics& g);
	CubeMapTexture* getSkyTexture(Graphics& g);
	void Initialize(Graphics& g);

private:
	bool initialized = false;
	std::unique_ptr<Sphere> sphere;
	std::unique_ptr<Mesh> skyMesh;          
	std::unique_ptr<CubeMapTexture> skyTexture;


};

