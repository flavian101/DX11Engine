#pragma once
#include <cstdint>

namespace DXEngine
{
    // How the numbers of an image are meant (see MaterialSlots.h for which slot uses which):
    //   SRGB   : colours (albedo, emissive). Stored gamma-encoded; the GPU converts to linear when sampling.
    //   Linear : data (normals, roughness, AO, ...). The numbers ARE the values; converting them would corrupt them.
    enum class TextureColorSpace : uint8_t { Linear, SRGB };
}
