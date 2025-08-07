#include "dxpch.h"
#include "Light.h"

namespace DXEngine {

	Light::Light()
		:m_LightColor(1.0, 0.85, 0.6, 1.0), m_lightDir(0.0f, -1.0f, 0.0f)
	{
	}

	DirectionalLight::DirectionalLight()
		:Light()
	{
		psBuffer.Initialize();
		psBuffer.data.d_Direction = m_lightDir;
		psBuffer.data.d_Color = m_LightColor;
		psBuffer.data.d_Ambient = DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
		psBuffer.data.d_Diffuse = DirectX::XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
		psBuffer.data.d_Enabled = 1;
	}

	DirectionalLight::~DirectionalLight()
	{
	}


	void DirectionalLight::Bind()
	{
		psBuffer.Update();
		RenderCommand::GetContext()->PSSetConstantBuffers(BindSlot::CB_Direction_Light, 1, psBuffer.GetAddressOf());
	}

	PointLight::PointLight()
		:Light()
	{
		psBuffer.Initialize();
		psBuffer.data.p_Color = m_LightColor;
		psBuffer.data.p_Position = DirectX::XMFLOAT3(0.0f, 10.0f, 0.0f);
		psBuffer.data.p_Attenuation = DirectX::XMFLOAT3(0.4f, 0.02f, 0.0f);
		psBuffer.data.P_Range = 50.0f;
		psBuffer.data.p_Enabled = 1;

	}

	PointLight::~PointLight()
	{
	}

	void PointLight::Bind()
	{
		psBuffer.Update();
		RenderCommand::GetContext()->PSSetConstantBuffers(BindSlot::CB_Point_Light, 1, psBuffer.GetAddressOf());

	}

	void PointLight::SetPosition(const DirectX::XMFLOAT3& pos)
	{
		psBuffer.data.p_Position = pos;

	}

	SpotLight::SpotLight()
		:Light()
	{
		psBuffer.Initialize();
		psBuffer.data.s_Color = m_LightColor;
		psBuffer.data.s_Direction = m_lightDir;
		psBuffer.data.s_Position = DirectX::XMFLOAT3(0.0f, 10.0f, 0.0f);
		psBuffer.data.s_Attenuation = DirectX::XMFLOAT3(0.4f, 0.02f, 0.0f);
		psBuffer.data.s_Range = 50.0f;
		psBuffer.data.s_Cone = 20.0f;
		psBuffer.data.s_Enabled = 1;
	}

	SpotLight::~SpotLight()
	{
	}

	void SpotLight::Bind()
	{
		psBuffer.Update();
		//TO-DO

		//psBuffer.data.light.spotPos.x = XMVectorGetX(camPos);
		//psBuffer.data.light.spotPos.y = XMVectorGetY(camPos);
		//psBuffer.data.light.spotPos.z = XMVectorGetZ(camPos);
		//
		////light dir
		//psBuffer.data.light.dir.x = XMVectorGetX(camTarget) - psBuffer.data.light.spotPos.x;
		//psBuffer.data.light.dir.y = XMVectorGetY(camTarget) - psBuffer.data.light.spotPos.y;
		//psBuffer.data.light.dir.z = XMVectorGetZ(camTarget) - psBuffer.data.light.spotPos.z;
		RenderCommand::GetContext()->PSSetConstantBuffers(BindSlot::CB_Spot_Light, 1, psBuffer.GetAddressOf());
	}
	void SpotLight::SetPosition(const DirectX::XMFLOAT3& pos)
	{
		psBuffer.data.s_Position = pos;

	}
}