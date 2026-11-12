#pragma once
#include "MaterialTypes.h"
#include <DirectXMath.h>
#include <cstddef>
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

	// This struct IS the constant buffer b2: the bytes are uploaded as they are and read by `cbuffer MaterialBufferData` in
	// common.hlsli, field by field in this order. Nothing checks that automatically, so the layout is pinned here: an added,
	// removed or reordered field fails the build instead of corrupting every material on screen.
	static_assert(sizeof(MaterialProperties) == 112, "MaterialProperties must stay 112 bytes (7 x float4) to match cbuffer MaterialBufferData");
	static_assert(sizeof(MaterialProperties) % 16 == 0, "a constant buffer is a multiple of 16 bytes");
	static_assert(offsetof(MaterialProperties, specularColor)    == 16, "MaterialProperties layout changed (specularColor)");
	static_assert(offsetof(MaterialProperties, emissiveColor)    == 32, "MaterialProperties layout changed (emissiveColor)");
	static_assert(offsetof(MaterialProperties, shininess)        == 48, "MaterialProperties layout changed (shininess)");
	static_assert(offsetof(MaterialProperties, metallic)         == 52, "MaterialProperties layout changed (metallic)");
	static_assert(offsetof(MaterialProperties, roughness)        == 56, "MaterialProperties layout changed (roughness)");
	static_assert(offsetof(MaterialProperties, alpha)            == 60, "MaterialProperties layout changed (alpha)");
	static_assert(offsetof(MaterialProperties, normalScale)      == 64, "MaterialProperties layout changed (normalScale)");
	static_assert(offsetof(MaterialProperties, heightScale)      == 68, "MaterialProperties layout changed (heightScale)");
	static_assert(offsetof(MaterialProperties, occlusionStrength)== 72, "MaterialProperties layout changed (occlusionStrength)");
	static_assert(offsetof(MaterialProperties, emissiveIntensity)== 76, "MaterialProperties layout changed (emissiveIntensity)");
	static_assert(offsetof(MaterialProperties, textureScale)     == 80, "MaterialProperties layout changed (textureScale)");
	static_assert(offsetof(MaterialProperties, textureOffset)    == 88, "MaterialProperties layout changed (textureOffset)");
	static_assert(offsetof(MaterialProperties, detailScale)      == 96, "MaterialProperties layout changed (detailScale)");
	static_assert(offsetof(MaterialProperties, detailOffset)     == 104, "MaterialProperties layout changed (detailOffset)");
}
