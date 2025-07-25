#pragma once
#include <DirectXMath.h>
#include "ConstantBuffer.h"
#include "Graphics.h"


class Light
{
public:
	Light(Graphics& gfx);
	virtual ~Light() = default;
	virtual void Bind(Graphics& gfx) = 0;
	
protected:
	DirectX::XMFLOAT4 m_LightColor;
	DirectX::XMFLOAT3 m_lightDir;
	int enabled = 0;
};


class DirectionalLight : Light
{
public:
	DirectionalLight(Graphics& gfx);
	virtual ~DirectionalLight();

	void Bind(Graphics& gfx) override;
private:
	ConstantBuffer<DirectionalLightData> psBuffer;

};

class PointLight : public Light
{
public:
	PointLight(Graphics& gfx);
	virtual ~PointLight();
	void Bind(Graphics& gfx) override;
	void SetPosition(const DirectX::XMFLOAT3& pos);
private:
	ConstantBuffer<PointLightData> psBuffer;

};

class SpotLight :public Light
{
public:
	SpotLight(Graphics& gfx);
	virtual ~SpotLight();

	void Bind(Graphics& gfx) override;
	void SetPosition(const DirectX::XMFLOAT3& pos);

private:
	ConstantBuffer<SpotLightData> psBuffer;

};
