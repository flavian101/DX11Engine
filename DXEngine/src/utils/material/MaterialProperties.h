#pragma once
#include "MaterialTypes.h"
#include <DirectXMath.h>
#include <memory>


namespace DXEngine
{
	class Texture;
	class CubeMapTexture;

	struct MaterialProperties
	{
		DirectX::XMFLOAT4 diffuseColor = { 1.0f,1.0f,1.0f,1.0f };
		DirectX::XMFLOAT4 specularColor = { 1.0f,1.0f,1.0f,1.0f };
		DirectX::XMFLOAT4 emissiveColor = { 0.0f,0.0f,0.0f,0.0f };

		float shininess = 32.0f;
		float meatallic = 0.0f;
		float roughness = 0.5f;
		float alpha = 1.0f;


		//Texture tiling and offset
		DirectX::XMFLOAT2 textureScale = { 1.0f,1.0f };
		DirectX::XMFLOAT2 textureOffset = { 0.0f,0.0f };

		//Material flag (packed into shader constants)
		uint32_t flags = 0;
		float padding[3] = { 0.0f };
	
	};

	struct MaterialResources
	{
		std::shared_ptr<Texture> diffuseTexture = nullptr;
		std::shared_ptr<Texture> normalTexture = nullptr;
		std::shared_ptr<Texture> specularTexture = nullptr;
		std::shared_ptr<Texture> emissiveTexture = nullptr;
		std::shared_ptr<CubeMapTexture> environmentTexture = nullptr;

		bool IsValid()const
		{
			//TO-DO basic material should have a diffuse color or a texture
			return true;
		}
	};
}