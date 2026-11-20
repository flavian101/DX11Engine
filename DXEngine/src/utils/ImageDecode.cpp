// (no dxpch.h on purpose: nothing here needs the engine, so it also builds in the GPU-free tests)
#include "ImageDecode.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

// The single translation unit that compiles stb_image (not static: tinyexr borrows its zlib decoder below).
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// tinyexr's ZIP / PXR24 compression needs a zlib. It uses the one inside stb_image: no zlib or miniz to add to the build
// (assimp's own bundled zip support defines miniz's symbols, and its zlib header needs a configure step). tinyexr references the
// COMPRESSING half of stb's zlib too, for saving EXR files; this decoder never saves one, so that half is an honest "not available".
#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
extern "C" unsigned char* stbi_zlib_compress(unsigned char*, int, int* outLength, int)
{
    if (outLength) *outLength = 0;
    return nullptr;                       // saving OpenEXR is not supported here: only decoding
}
#include "tinyexr.h"

namespace DXEngine::ImageDecode
{
    namespace
    {
        bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& bytes, std::string& error)
        {
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in) { error = "the file cannot be opened"; return false; }
            const std::streamsize size = in.tellg();
            if (size <= 0) { error = "the file is empty"; return false; }
            bytes.resize(static_cast<size_t>(size));
            in.seekg(0);
            in.read(reinterpret_cast<char*>(bytes.data()), size);
            if (!in) { error = "the file cannot be read"; return false; }
            return true;
        }

        uint8_t ToByte(float value, TextureColorSpace space)
        {
            if (!(value > 0.0f)) return 0;                              // also NaN
            if (value >= 1.0f) return 255;
            if (space == TextureColorSpace::SRGB) return EncodeSRGB(value);
            return static_cast<uint8_t>(value * 255.0f + 0.5f);
        }

        Image FromFloats(const float* rgba, int width, int height, TextureColorSpace space)
        {
            Image image;
            image.width = width; image.height = height; image.hdrSource = true;
            image.rgba.resize(size_t(width) * height * 4);
            for (size_t i = 0; i < size_t(width) * height; ++i)
            {
                for (int c = 0; c < 3; ++c) image.rgba[i * 4 + c] = ToByte(rgba[i * 4 + c], space);
                const float a = rgba[i * 4 + 3];
                image.rgba[i * 4 + 3] = a > 0.0f ? ToByte(a, TextureColorSpace::Linear) : 0;   // alpha is never gamma encoded
            }
            return image;
        }

        bool IsExr(const uint8_t* data, size_t size)
        {
            return size >= 4 && data[0] == 0x76 && data[1] == 0x2f && data[2] == 0x31 && data[3] == 0x01;   // the OpenEXR magic number
        }

        Image DecodeExr(const uint8_t* data, size_t size, const std::string& name, TextureColorSpace space)
        {
            Image image;
            float* pixels = nullptr; int width = 0, height = 0; const char* error = nullptr;
            const int result = LoadEXRFromMemory(&pixels, &width, &height, data, size, &error);
            if (result != TINYEXR_SUCCESS)
            {
                const std::string compression = ExrCompressionName(data, size);
                std::string why = error ? error : "unknown error";
                if (error) FreeEXRErrorMessage(error);
                if (compression == "DWAA" || compression == "DWAB")
                    image.error = "the OpenEXR file '" + name + "' uses " + compression + " compression, which cannot be decoded here "
                                  "(supported: none, RLE, ZIP, PIZ, PXR24, B44). Re-export it with ZIP or PIZ, or as a PNG.";
                else
                    image.error = "the OpenEXR file '" + name + "' cannot be decoded: " + why;
                return image;
            }
            image = FromFloats(pixels, width, height, space);
            free(pixels);
            return image;
        }
    }

    uint8_t EncodeSRGB(float linear)
    {
        const float v = std::clamp(linear, 0.0f, 1.0f);
        const float encoded = v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
        return static_cast<uint8_t>(std::clamp(encoded, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    std::string ExrCompressionName(const uint8_t* data, size_t size)
    {
        if (!IsExr(data, size)) return {};
        static const char kKey[] = "compression";
        const size_t keyLen = sizeof(kKey);                       // with the terminating zero
        const size_t limit = std::min<size_t>(size, 16384);        // the header is at the start
        for (size_t i = 8; i + 2 * keyLen + 5 < limit; ++i)
        {
            // an attribute is: name\0 type\0 int32 size, value. Here both the name and the type are "compression".
            if (std::memcmp(data + i, kKey, keyLen) != 0 || std::memcmp(data + i + keyLen, kKey, keyLen) != 0) continue;
            const uint8_t value = data[i + 2 * keyLen + 4];
            static const char* kNames[] = { "NONE", "RLE", "ZIPS", "ZIP", "PIZ", "PXR24", "B44", "B44A", "DWAA", "DWAB" };
            return value < 10 ? kNames[value] : "unknown";
        }
        return "unknown";
    }

    Image DecodeMemory(const uint8_t* data, size_t size, const std::string& name, TextureColorSpace colorSpace)
    {
        Image image;
        if (!data || size == 0) { image.error = "'" + name + "' has no data"; return image; }

        if (IsExr(data, size))
            return DecodeExr(data, size, name, colorSpace);

        const int bytes = static_cast<int>(std::min<size_t>(size, 0x7fffffff));
        if (stbi_is_hdr_from_memory(data, bytes))
        {
            int w = 0, h = 0, channels = 0;
            float* pixels = stbi_loadf_from_memory(data, bytes, &w, &h, &channels, 4);
            if (!pixels) { image.error = "could not decode '" + name + "': " + stbi_failure_reason(); return image; }
            image = FromFloats(pixels, w, h, colorSpace);
            stbi_image_free(pixels);
            return image;
        }

        int w = 0, h = 0, channels = 0;
        stbi_uc* decoded = stbi_load_from_memory(data, bytes, &w, &h, &channels, STBI_rgb_alpha);
        if (!decoded) { image.error = "could not decode '" + name + "': " + stbi_failure_reason(); return image; }
        image.width = w; image.height = h;
        image.rgba.assign(decoded, decoded + size_t(w) * h * 4);
        stbi_image_free(decoded);
        return image;
    }

    Image DecodeFile(const std::string& path, TextureColorSpace colorSpace)
    {
        std::vector<uint8_t> bytes;
        Image failed;
        if (!ReadFileBytes(path, bytes, failed.error))
        {
            failed.error = "could not load '" + path + "': " + failed.error;
            return failed;
        }
        return DecodeMemory(bytes.data(), bytes.size(), path, colorSpace);
    }
}
