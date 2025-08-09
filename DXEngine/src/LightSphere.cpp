#include "dxpch.h"
#include "LightSphere.h"
#include "utils/Texture.h"


namespace DXEngine {


LightSphere::LightSphere()
	:Model(),
	m_Sphere(64)
{
	m_Mesh = std::make_shared<Mesh>(m_Sphere.getMeshResource());
	auto flat = MaterialFactory::CreateEmissiveMaterial("TestUnlit");


	m_Mesh->SetMaterial(flat);
	m_Light = std::make_shared<PointLight>();
	flat->SetEmissiveColor(m_Light->GetLightColor());
	flat->SetDiffuseColor(m_Light->GetLightColor());

}
void LightSphere::BindLight()
{
	m_Light->Bind();
}

//void LightSphere::Render()
//{
//	DirectX::XMFLOAT3 lightPos = { DirectX::XMVectorGetX(GetTranslation()),DirectX::XMVectorGetY(GetTranslation()),DirectX::XMVectorGetZ(GetTranslation()) };
//	m_Light->SetPosition(lightPos);
//	m_Light->Bind();
//	Model::Render();
//}
}
