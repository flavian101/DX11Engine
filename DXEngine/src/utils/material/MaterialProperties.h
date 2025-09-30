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
		DirectX::XMFLOAT4 emissiveColor = { 0.0f,0.0f,0.0f,1.0f };

		float shininess = 32.0f;
		float metallic = 0.0f;
		float roughness = 0.5f;
		float alpha = 1.0f;

		float normalScale = 1.0f;
		float heightScale = 0.05f;
		float occlusionStrength = 1.0f;
		float emissiveIntensity = 3.0f;


		//Texture tiling and offset
		DirectX::XMFLOAT2 textureScale = { 1.0f,1.0f };
		DirectX::XMFLOAT2 textureOffset = { 0.0f,0.0f };
		DirectX::XMFLOAT2 detailScale = { 8.0f, 8.0f };    
		DirectX::XMFLOAT2 detailOffset = { 0.0f, 0.0f };

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

		//PBR textures
		std::shared_ptr<Texture> roughnessTexture = nullptr;
		std::shared_ptr<Texture> metallicTexture = nullptr;
		std::shared_ptr<Texture> aoTexture = nullptr;
		std::shared_ptr<Texture> heightTexture = nullptr;
		std::shared_ptr<Texture> opacityTexture = nullptr;

		// NEW: Detail textures
		std::shared_ptr<Texture> detailDiffuseTexture = nullptr;
		std::shared_ptr<Texture> detailNormalTexture = nullptr;


		bool IsValid()const
		{
			//TO-DO basic material should have a diffuse color or a texture
			return true;
		}

		bool HasAnyTextures() const
		{
			return diffuseTexture || normalTexture || specularTexture ||
				roughnessTexture || metallicTexture || emissiveTexture ||
				aoTexture || heightTexture || opacityTexture ||
				detailDiffuseTexture || detailNormalTexture ||
				environmentTexture;
		}

		size_t GetTextureCount() const
		{
			size_t count = 0;
			if (diffuseTexture) count++;
			if (normalTexture) count++;
			if (specularTexture) count++;
			if (emissiveTexture) count++;
			if (roughnessTexture) count++;
			if (metallicTexture) count++;
			if (aoTexture) count++;
			if (heightTexture) count++;
			if (opacityTexture) count++;
			if (detailDiffuseTexture) count++;
			if (detailNormalTexture) count++;
			if (environmentTexture) count++;
			return count;
		}
	};
}