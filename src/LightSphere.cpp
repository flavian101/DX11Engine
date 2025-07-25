#include "LightSphere.h"

LightSphere::LightSphere(Graphics& gfx)
	:
	m_Sphere(64)

{
	m_Mesh = std::make_shared<Mesh>(gfx, m_Sphere.getIndices(), m_Sphere.getVertex(),
		L"assets/shaders/flatVsShader.cso",
		L"assets/shaders/flatPsShader.cso");
	m_Light = std::make_shared<PointLight>(gfx);

	m_ModelMatrix = XMMatrixIdentity();
}

void LightSphere::Draw(Graphics& gfx)
{
	m_Scale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	m_Translation = XMMatrixTranslation(0.0f, 10.0f, 0.0f);

	m_ModelMatrix = m_Scale * m_Translation;

	m_Light->Bind(gfx);
	m_Mesh->Draw(gfx, m_ModelMatrix, {0.0f}, {0.0f});
}
