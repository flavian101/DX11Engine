#pragma once
#include "ConstantBuffer.h"
#include "Sampler.h"
#include "Texture.h"
#include <vector>
#include <shaders/ShaderProgram.h>
#include "CubeMapTexture.h"

namespace DXEngine {

	enum TextureSlot : UINT
	{
		Diffuse = 0,
		NormalMap = 1
	};
	class Material
	{
	public:
		Material();

		~Material();
		void Bind();
		void SetShaderProgram(std::shared_ptr<ShaderProgram> program);

		void SetDiffuseColor(const DirectX::XMFLOAT4& diffuse);
		void SetDiffuse(const std::shared_ptr<Texture>& diffuseTexture);
		void SetNormalMap(const std::shared_ptr<Texture>& normalTexture);
		void SetSkyMaterial(const std::shared_ptr<CubeMapTexture>& cubemap);

	private:
		Sampler samp;
		std::shared_ptr<Texture> m_Diffuse = nullptr;
		std::shared_ptr<Texture> m_NormalMap = nullptr;
		std::shared_ptr<CubeMapTexture> m_CubeMap = nullptr;
		bool isCubemap = false;
		ConstantBuffer<MaterialBufferData> psBuffer;
		std::shared_ptr<ShaderProgram> m_Program = nullptr;
	};

}