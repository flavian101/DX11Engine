#pragma once
#include "MaterialSlots.h"
#include <string>

namespace DXEngine
{
	// material Type enumaeration for different Rendering Techniques
	enum class MaterialType
	{
		Unlit,
		Lit,
		PBR,           //Physically Based Rendering
		Transparent,
		UI,
		Skybox,
		Emissive,
	};

	// (TextureSlot lives in MaterialSlots.h: the slot table is the one place that describes a texture slot.)



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
