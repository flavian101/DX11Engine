#pragma once
#include "MaterialTypes.h"
#include <DirectXMath.h>
#include <memory>


namespace DXEngine
{

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
	
	};
}