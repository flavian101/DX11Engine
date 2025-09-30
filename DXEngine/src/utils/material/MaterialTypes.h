#pragma once
#include <string>

namespace DXEngine
{
	// material Type enumaeration for different Rendering Techniques
	enum class MaterialType
	{
		Unlit,
		Lit,
		PBR,           //Physically Based Rendering
		Skybox,
		Transparent,
		Emissive,
		UI,
		Terrain,       //For terrain materials
		Vegetation,    //For plants/grass with wind
		Water,         //For water surfaces
		Glass          //For glass materials
	};

	//material config flags 
	enum MaterialFlags : uint32_t
	{
		NoneMaterialFlag = 0,
		HasDiffuseTexture = 1 << 0,
		HasNormalMap = 1 << 1,
		HasSpecularMap = 1 << 2,
		HasEmissiveMap = 1 << 3,
		HasRoughnessMap = 1 << 4,      //
		HasMetallicMap = 1 << 5,       //
		HasAOMap = 1 << 6,             // Ambient Occlusion
		HasHeightMap = 1 << 7,         // Height/Displacement
		HasDetailMap = 1 << 8,         // Detail textures
		HasOpacityMap = 1 << 9,        // Alpha channel
		HasSubsurfaceMap = 1 << 10,    // Subsurface scattering
		HasAnisotropyMap = 1 << 11,    // Anisotropic reflections
		HasClearcoatMap = 1 << 12,     // Clear coat laye
		HasEnvironmentMap = 1<<13,
		IsTransparent = 1 << 14,
		IsTwoSided = 1 << 15,
		CastsShadows = 1 << 16,
		ReceivesShadows = 1 << 17,
		UseParallaxMapping = 1 << 18,  // Parallax occlusion
		UseAlphaTest = 1 << 19,        // Alpha testing
		UseDetailTextures = 1 << 20    // Detail texture blending
	};

	// Texture Binding slots 
	enum class TextureSlot : uint32_t
	{
		// === CORE PBR TEXTURES ===
		Diffuse = 0,   // diffuseTexture        (t0)
		Normal = 1,   // normalTexture         (t1)
		Specular = 2,   // specularTexture       (t2)
		Emissive = 3,   // emissiveTexture       (t3)

		// === PBR EXTENSIONS ===
		Roughness = 4,   // roughnessTexture      (t4)
		Metallic = 5,   // metallicTexture       (t5)
		AmbientOcclusion = 6,   // aoTexture             (t6)
		Height = 7,   // heightTexture         (t7)

		// === ADDITIONAL DETAIL ===
		Opacity = 8,   // opacityTexture        (t8)
		DetailDiffuse = 9,   // detailDiffuseTexture  (t9)
		DetailNormal = 10,  // detailNormalTexture   (t10)

		// === ENVIRONMENT MAPPING ===
		Environment = 11,  // environmentTexture    (t11)
		Irradiance = 12,  // irradianceTexture     (t12)
		BRDF_LUT = 13,  // brdfLUTTexture        (t13)

		// === SHADOWS ===
		Shadow = 14,  // shadowMaps            (t14)

		// === SPECIALIZED (future use) ===
		Subsurface = 15,  // subsurfaceTexture     (t15)
		Anisotropy = 16,  // anisotropyTexture     (t16)
		Clearcoat = 17,  // clearcoatTexture      (t17)
		ClearcoatRoughness = 18,  // clearcoatRoughnessTex (t18)

		MaxTextureSlots = 32
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
