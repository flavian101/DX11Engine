#pragma once
#include "material/MaterialSlots.h"     // TextureColorSpace
#include <cstdint>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // CPU mip-chain generation for RGBA8 images, colour-space aware. Pure: no GPU, no files (unit-tested).
    //
    // Why the colour space matters: an sRGB texture stores gamma-encoded values, and the GPU averages the LINEAR values
    // when it filters. A mip level made by averaging the encoded bytes is darker than that (a black/white checkerboard
    // would shrink to 128 instead of the 188 that is half the light), so a distant tiled floor would look too dark.
    // For sRGB data the downsample therefore decodes, averages and re-encodes. Data textures (normals, roughness, ...)
    // are averaged as plain numbers. Alpha is never gamma-encoded and is always averaged directly.
    // ==================================================================
    namespace ImageMips
    {
        float   SrgbToLinear(uint8_t encoded);        // exact sRGB EOTF: 0..255 -> 0..1
        uint8_t LinearToSrgb(float linear);           // exact sRGB OETF, rounded and clamped: 0..1 -> 0..255

        // Number of levels of a full chain: 1 + floor(log2(max(w, h))).
        uint32_t MipLevelCount(uint32_t width, uint32_t height);

        // `pixels` holds level 0 (width * height * 4 bytes). Appends levels 1..N behind it, each max(1, size / 2) in both
        // directions (the rule D3D11Texture uses for the initial data), and returns the level count.
        uint32_t AppendMipChain(std::vector<uint8_t>& pixels, uint32_t width, uint32_t height, TextureColorSpace colorSpace);
    }
}
