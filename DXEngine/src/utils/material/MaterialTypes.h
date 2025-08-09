#pragma once
#include <string>

namespace DXEngine
{
	// material Type enumaeration for different Rendering Techniques
	enum class MaterialType
	{
		Unlit,
		Lit,
		LitTextured,
		LitNormalMapped,
		Skybox,
		Transparent,
		Emissive,
		PBR,
		UI,
	};

	//material config flags 
	enum MaterialFlags : uint32_t
	{
		NoneMaterialFlag = 0,
		HasDiffuseTexture = 1 << 0,
		HasNormalMap = 1 << 1,
		HasSpecularMap = 1 << 2,
		HasEmissiveMap = 1 << 3,
		IsTransparent = 1 << 4,
		IsTwoSided = 1 << 5 ,
		castsShadows = 1 << 6 ,
		ReceivesShadows = 1 << 7,
	};
	// Texture Binding slots 
	enum class TextureSlot: uint32_t
	{
		Diffuse = 0,
		Normal = 1,
		Specular = 2,
		Emissive = 3,
		Environment = 4,
		Shadow = 5
	};

	//render Queue for draw order optimization
	enum class RenderQueue : uint32_t
	{
		Background = 1000,
		Opaque = 2000,
		Transparent = 3000,
		UI = 4000,
		Overlay = 5000
	};

}
