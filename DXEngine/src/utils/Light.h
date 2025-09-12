#pragma once
#include "Buffer.h"
#include "renderer/RendererCommand.h"

namespace DXEngine {

	class Light
	{
	public:
		Light();
		virtual ~Light() = default;
		virtual void Bind() = 0;
		const DirectX::XMFLOAT4& GetLightColor() { return m_LightColor; }

	protected:
		DirectX::XMFLOAT4 m_LightColor;
		DirectX::XMFLOAT3 m_lightDir;
		int enabled = 0;
	};


	class DirectionalLight : public Light
	{
	public:
		DirectionalLight();
		virtual ~DirectionalLight();

		void Bind() override;
	private:
		ConstantBuffer<DirectionalLightData> psBuffer;
		std::shared_ptr<DirectionalLightData> directionalData;

	};

	class PointLight : public Light
	{
	public:
		PointLight();
		virtual ~PointLight();
		void Bind() override;
		void SetPosition(const DirectX::XMFLOAT3& pos);
	private:
		ConstantBuffer<PointLightData> psBuffer;
		std::shared_ptr<PointLightData> pointData;

	};

	class SpotLight :public Light
	{
	public:
		SpotLight();
		virtual ~SpotLight();

		void Bind() override;
		void SetPosition(const DirectX::XMFLOAT3& pos);

	private:
		ConstantBuffer<SpotLightData> psBuffer;
		std::shared_ptr<SpotLightData> spotData;

	};
}