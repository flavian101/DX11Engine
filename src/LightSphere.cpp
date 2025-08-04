#include "LightSphere.h"

namespace DXEngine {


LightSphere::LightSphere(Graphics& gfx, std::shared_ptr<ShaderProgram> program)
	:Model(gfx, program),
	m_Sphere(64)
{
	m_Mesh = std::make_shared<Mesh>(gfx, m_Sphere.getMeshResource());
	auto flat = std::make_shared<Material>(gfx);
	flat->SetShaderProgram(program);
	m_Mesh->SetMaterial(flat);
	m_Light = std::make_shared<PointLight>(gfx);
}

void LightSphere::Render(Graphics& gfx)
{
	DirectX::XMFLOAT3 lightPos = { DirectX::XMVectorGetX(GetTranslation()),DirectX::XMVectorGetY(GetTranslation()),DirectX::XMVectorGetZ(GetTranslation()) };
	m_Light->SetPosition(lightPos);
	m_Light->Bind(gfx);
	Model::Render(gfx);
}
}
