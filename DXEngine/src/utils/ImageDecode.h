#pragma once
// ============================================================================
// ImageDecode: image file (or bytes) -> 8-bit RGBA pixels. CPU only: no GPU, no engine types, so it is tested natively
// (tests/image_decode_tests.cpp). ImageLoader turns the pixels into a texture.
//
// Formats: everything stb_image reads (PNG, JPEG, BMP, TGA, GIF, PSD, PIC, PNM), Radiance .hdr, and OpenEXR (tinyexr:
// uncompressed, RLE, ZIP, ZIPS, PIZ, PXR24, B44). NOT OpenEXR's lossy DWAA / DWAB compression: the decode then FAILS with a message
// that says so, and how to get the image into a readable form. (Poly Haven's EXR downloads use DWAA.)
//
// High dynamic range images (.hdr, .exr) are stored as 8 bits for now: values are clamped to [0, 1] (what an LDR map such
// as a normal or roughness map needs). A float texture path for HDR environments comes with image based lighting.
// A SRGB colour space encodes the (linear) float values to sRGB; Linear keeps them as they are.
// ============================================================================
#include "TextureColorSpace.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace DXEngine::ImageDecode
{
    struct Image
    {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> rgba;       // width * height * 4
        bool hdrSource = false;          // the file held floating point data (clamped to 8 bits)
        std::string error;               // why it failed (empty on success)

        bool ok() const { return error.empty() && width > 0 && height > 0 && rgba.size() == size_t(width) * height * 4; }
    };

    // 'colorSpace' only matters for floating point sources (see above); 8-bit files are returned as stored.
    Image DecodeFile(const std::string& path, TextureColorSpace colorSpace);
    Image DecodeMemory(const uint8_t* data, size_t size, const std::string& name, TextureColorSpace colorSpace);

    // The compression named in an OpenEXR header ("ZIP", "DWAA", ...), or "" when 'data' is not an EXR file.
    std::string ExrCompressionName(const uint8_t* data, size_t size);

    // linear [0,1] -> sRGB [0,255] (the exact transfer function, not a gamma 2.2 approximation)
    uint8_t EncodeSRGB(float linear);
}
