#pragma once
#include "ConstantBuffer.h"
#include "Sampler.h"
#include "Texture.h"
#include <vector>

enum TextureSlot : UINT
{
	Diffuse = 0,
	NormalMap = 1
};
class Material
{
public:
	Material(Graphics& gfx);

	~Material();
	void Bind(Graphics& gfx);

	void SetDiffuseColor(const XMFLOAT4& diffuse);
	void SetDiffuse(const std::shared_ptr<Texture>& diffuseTexture);
	void SetNormalMap(const std::shared_ptr<Texture>& normalTexture);



private:
	Sampler samp;
	std::shared_ptr<Texture> m_Diffuse = nullptr;
	std::shared_ptr<Texture> m_NormalMap = nullptr;
	ConstantBuffer<MaterialBufferData> psBuffer;
};

