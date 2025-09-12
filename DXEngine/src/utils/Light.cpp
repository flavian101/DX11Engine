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
		directionalData = std::make_shared<DirectionalLightData>();

		directionalData->d_Direction = m_lightDir;
		directionalData->d_Color = m_LightColor;
		directionalData->d_Ambient = DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
		directionalData->d_Diffuse = DirectX::XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
		directionalData->d_Enabled = 1;

		psBuffer.Initialize(directionalData.get());
	}

	DirectionalLight::~DirectionalLight()
	{
	}


	void DirectionalLight::Bind()
	{
		psBuffer.Update(*directionalData.get());
		RenderCommand::GetContext()->PSSetConstantBuffers(BindSlot::CB_Direction_Light, 1, psBuffer.GetAddressOf());
	}

	PointLight::PointLight()
		:Light()
	{
		pointData = std::make_shared<PointLightData>();
		pointData->p_Color = m_LightColor;
		pointData->p_Position = DirectX::XMFLOAT3(0.0f, 10.0f, 0.0f);
		pointData->p_Attenuation = DirectX::XMFLOAT3(0.4f, 0.02f, 0.0f);
		pointData->P_Range = 50.0f;
		pointData->p_Enabled = 1;

		psBuffer.Initialize(pointData.get());

	}

	PointLight::~PointLight()
	{
	}

	void PointLight::Bind()
	{
		psBuffer.Update(*pointData.get());
		RenderCommand::GetContext()->PSSetConstantBuffers(BindSlot::CB_Point_Light, 1, psBuffer.GetAddressOf());

	}

	void PointLight::SetPosition(const DirectX::XMFLOAT3& pos)
	{
		pointData->p_Position = pos;

	}

	SpotLight::SpotLight()
		:Light()
	{
		spotData->s_Color = m_LightColor;
		spotData->s_Direction = m_lightDir;
		spotData->s_Position = DirectX::XMFLOAT3(0.0f, 10.0f, 0.0f);
		spotData->s_Attenuation = DirectX::XMFLOAT3(0.4f, 0.02f, 0.0f);
		spotData->s_Range = 50.0f;
		spotData->s_Cone = 20.0f;
		spotData->s_Enabled = 1;

		psBuffer.Initialize(spotData.get());
	}

	SpotLight::~SpotLight()
	{
	}

	void SpotLight::Bind()
	{
		psBuffer.Update(*spotData.get());
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
		spotData->s_Position = pos;

	}
}