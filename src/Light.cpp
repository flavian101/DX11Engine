#include "Light.h"


Light::Light(Graphics& gfx)
	:m_LightColor(1.0, 0.85, 0.6, 1.0), m_lightDir(0.0f, -1.0f, 0.0f)
{}

DirectionalLight::DirectionalLight(Graphics& gfx)
	:Light(gfx)
{
	psBuffer.Initialize(gfx);
	psBuffer.data.d_Direction = m_lightDir;
	psBuffer.data.d_Color = m_LightColor;
	psBuffer.data.d_Ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	psBuffer.data.d_Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	psBuffer.data.d_Enabled = 1;
}

DirectionalLight::~DirectionalLight()
{
}


void DirectionalLight::Bind(Graphics& gfx)
{
	psBuffer.Update(gfx);
	gfx.GetContext()->PSSetConstantBuffers(BindSlot::CB_Direction_Light, 1, psBuffer.GetAddressOf());
}

PointLight::PointLight(Graphics& gfx)
	:Light(gfx)
{
	psBuffer.Initialize(gfx);
	psBuffer.data.p_Color = m_LightColor;
	psBuffer.data.p_Position = XMFLOAT3(0.0f, 10.0f, 0.0f);
	psBuffer.data.p_Attenuation = XMFLOAT3(0.4f, 0.02f, 0.0f);
	psBuffer.data.P_Range = 50.0f;
	psBuffer.data.p_Enabled = 1;

}

PointLight::~PointLight()
{
}

void PointLight::Bind(Graphics& gfx)
{
	psBuffer.Update(gfx);
	gfx.GetContext()->PSSetConstantBuffers(BindSlot::CB_Point_Light, 1, psBuffer.GetAddressOf());

}

void PointLight::SetPosition(const DirectX::XMFLOAT3& pos)
{
	psBuffer.data.p_Position = pos;

}

SpotLight::SpotLight(Graphics& gfx)
	:Light(gfx)
{
	psBuffer.Initialize(gfx);
	psBuffer.data.s_Color = m_LightColor;
	psBuffer.data.s_Direction = m_lightDir;
	psBuffer.data.s_Position = XMFLOAT3(0.0f, 10.0f, 0.0f);
	psBuffer.data.s_Attenuation = XMFLOAT3(0.4f, 0.02f, 0.0f);
	psBuffer.data.s_Range = 50.0f;
	psBuffer.data.s_Cone = 20.0f;
	psBuffer.data.s_Enabled = 1;
}

SpotLight::~SpotLight()
{
}

void SpotLight::Bind(Graphics& gfx)
{
	psBuffer.Update(gfx);
	gfx.GetContext()->PSSetConstantBuffers(BindSlot::CB_Spot_Light, 1, psBuffer.GetAddressOf());
}
void SpotLight::SetPosition(const DirectX::XMFLOAT3& pos)
{
	psBuffer.data.s_Position = pos;

}
