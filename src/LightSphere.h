#pragma once
#include "Light.h"
#include "Sphere.h"
#include "Mesh.h"

class LightSphere
{
public:
	LightSphere(Graphics& gfx);

	void Draw(Graphics& gfx);


private:
	Sphere m_Sphere;
	std::shared_ptr<Mesh> m_Mesh;
	std::shared_ptr <PointLight> m_Light;

	XMMATRIX m_ModelMatrix;
	DirectX::XMMATRIX m_Scale;
	DirectX::XMMATRIX m_Translation;
};

