#include "dxpch.h"
#include "ImageLoader.h"
#include "ImageMips.h"

// The single translation unit that compiles stb_image (the legacy Texture.cpp no longer does).
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <vector>

namespace DXEngine
{
    namespace
    {
        void Log(const std::string& msg) { OutputDebugStringA(("[ImageLoader] " + msg + "\n").c_str()); }

        // sRGB data is stored in an RGBA8_SRGB texture: the GPU then converts to linear when it samples (and filters in
        // linear). Everything else is plain RGBA8_UNORM.
        RHI::TextureFormat FormatFor(TextureColorSpace colorSpace)
        {
            return colorSpace == TextureColorSpace::SRGB ? RHI::TextureFormat::RGBA8_SRGB : RHI::TextureFormat::RGBA8_UNORM;
        }

        std::shared_ptr<RHI::ITexture> CreateTexture(RHI::IGraphicsDevice& device, const RHI::TextureDesc& desc)
        {
            try
            {
                auto texture = device.CreateTexture(desc);
                if (!texture) Log("device failed to create texture '" + desc.debugName + "'");
                return texture;
            }
            catch (const std::exception& e)      // D3D11Texture reports failure by throwing
            {
                Log("creating '" + desc.debugName + "' failed: " + e.what());
                return nullptr;
            }
        }
    }

    namespace
    {
        // `pixels` holds level 0 (RGBA8). Builds the mip chain in place and creates the texture.
        std::shared_ptr<RHI::ITexture> CreateFromRGBA8(RHI::IGraphicsDevice& device,
                                                       std::vector<uint8_t>& pixels,
                                                       uint32_t width, uint32_t height,
                                                       const std::string& debugName, TextureColorSpace colorSpace,
                                                       bool generateMipChain)
        {
            RHI::TextureDesc desc;
            desc.type      = RHI::TextureType::Texture2D;
            desc.format    = FormatFor(colorSpace);
            desc.width     = width;
            desc.height    = height;
            desc.mipLevels = generateMipChain ? ImageMips::AppendMipChain(pixels, width, height, colorSpace) : 1;
            desc.initialData = pixels.data();    // complete chain, contiguous: exactly what D3D11Texture expects
            desc.debugName = debugName;
            return CreateTexture(device, desc);
        }
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::LoadTexture2D(RHI::IGraphicsDevice& device,
                                                              const std::string& path,
                                                              TextureColorSpace colorSpace,
                                                              bool generateMipChain)
    {
        int width = 0, height = 0, channels = 0;
        stbi_uc* decoded = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!decoded)
        {
            Log("could not load '" + path + "': " + stbi_failure_reason());
            return nullptr;
        }

        std::vector<uint8_t> pixels(decoded, decoded + static_cast<size_t>(width) * height * 4);
        stbi_image_free(decoded);
        return CreateFromRGBA8(device, pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), path, colorSpace, generateMipChain);
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::LoadTexture2DFromMemory(RHI::IGraphicsDevice& device,
                                                                        const uint8_t* data, size_t sizeBytes,
                                                                        const std::string& debugName,
                                                                        TextureColorSpace colorSpace,
                                                                        bool generateMipChain)
    {
        int width = 0, height = 0, channels = 0;
        stbi_uc* decoded = stbi_load_from_memory(data, static_cast<int>(sizeBytes), &width, &height, &channels, STBI_rgb_alpha);
        if (!decoded)
        {
            Log("could not decode '" + debugName + "': " + stbi_failure_reason());
            return nullptr;
        }

        std::vector<uint8_t> pixels(decoded, decoded + static_cast<size_t>(width) * height * 4);
        stbi_image_free(decoded);
        return CreateFromRGBA8(device, pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), debugName, colorSpace, generateMipChain);
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::CreateTexture2DFromRGBA8(RHI::IGraphicsDevice& device,
                                                                         const uint8_t* rgba,
                                                                         uint32_t width, uint32_t height,
                                                                         const std::string& debugName,
                                                                         TextureColorSpace colorSpace,
                                                                         bool generateMipChain)
    {
        if (!rgba || width == 0 || height == 0)
        {
            Log("invalid pixel data for '" + debugName + "'");
            return nullptr;
        }
        std::vector<uint8_t> pixels(rgba, rgba + static_cast<size_t>(width) * height * 4);
        return CreateFromRGBA8(device, pixels, width, height, debugName, colorSpace, generateMipChain);
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::LoadCubeMap(RHI::IGraphicsDevice& device,
                                                            const std::array<std::string, 6>& facePaths,
                                                            TextureColorSpace colorSpace)
    {
        std::vector<uint8_t> pixels;
        int width = 0, height = 0;

        for (size_t i = 0; i < 6; ++i)
        {
            int w = 0, h = 0, channels = 0;
            stbi_uc* decoded = stbi_load(facePaths[i].c_str(), &w, &h, &channels, STBI_rgb_alpha);
            if (!decoded)
            {
                Log("could not load cube face '" + facePaths[i] + "': " + stbi_failure_reason());
                return nullptr;
            }
            if (i == 0) { width = w; height = h; pixels.reserve(static_cast<size_t>(w) * h * 4 * 6); }
            if (w != width || h != height)
            {
                Log("cube face '" + facePaths[i] + "' has a different size than the first face");
                stbi_image_free(decoded);
                return nullptr;
            }
            pixels.insert(pixels.end(), decoded, decoded + static_cast<size_t>(w) * h * 4);
            stbi_image_free(decoded);
        }

        RHI::TextureDesc desc;
        desc.type        = RHI::TextureType::TextureCube;
        desc.format      = FormatFor(colorSpace);
        desc.width       = static_cast<uint32_t>(width);
        desc.height      = static_cast<uint32_t>(height);
        desc.mipLevels   = 1;
        desc.arraySize   = 1;                 // the RHI multiplies by 6 for cube maps
        desc.initialData = pixels.data();     // six faces back to back, face i = array slice i
        desc.debugName   = facePaths[0] + " (+5 faces)";

        return CreateTexture(device, desc);
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::CreateSolidColor(RHI::IGraphicsDevice& device,
                                                                 uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                                                 TextureColorSpace colorSpace)
    {
        const uint8_t pixel[4] = { r, g, b, a };

        RHI::TextureDesc desc;
        desc.type        = RHI::TextureType::Texture2D;
        desc.format      = FormatFor(colorSpace);
        desc.width       = 1;
        desc.height      = 1;
        desc.mipLevels   = 1;
        desc.initialData = pixel;
        desc.debugName   = "SolidColor";

        return CreateTexture(device, desc);
    }

    // ---- the slot table decides the colour space ----
    std::shared_ptr<RHI::ITexture> ImageLoader::LoadForSlot(RHI::IGraphicsDevice& device, const std::string& path,
                                                            TextureSlot slot, bool generateMipChain)
    {
        return LoadTexture2D(device, path, MaterialSlots::Info(slot).colorSpace, generateMipChain);
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::LoadCubeMapForSlot(RHI::IGraphicsDevice& device,
                                                                   const std::array<std::string, 6>& facePaths, TextureSlot slot)
    {
        return LoadCubeMap(device, facePaths, MaterialSlots::Info(slot).colorSpace);
    }
}
