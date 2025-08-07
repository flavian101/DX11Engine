#include "dxpch.h"
#include "LightSphere.h"

namespace DXEngine {


LightSphere::LightSphere(const std::shared_ptr<ShaderProgram>& program)
	:Model(program),
	m_Sphere(64)
{
	m_Mesh = std::make_shared<Mesh>(m_Sphere.getMeshResource());
	auto flat = std::make_shared<Material>();
	flat->SetShaderProgram(program);
	m_Mesh->SetMaterial(flat);
	m_Light = std::make_shared<PointLight>();
}

//void LightSphere::Render()
//{
//	DirectX::XMFLOAT3 lightPos = { DirectX::XMVectorGetX(GetTranslation()),DirectX::XMVectorGetY(GetTranslation()),DirectX::XMVectorGetZ(GetTranslation()) };
//	m_Light->SetPosition(lightPos);
//	m_Light->Bind();
//	Model::Render();
//}
}
