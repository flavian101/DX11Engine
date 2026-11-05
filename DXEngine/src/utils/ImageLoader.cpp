#include "dxpch.h"
#include "ImageLoader.h"

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

        // Appends mip levels 1..N to `pixels` (which holds level 0 on entry) and returns the level count.
        // Level sizes follow the same rule as D3D11Texture's initial-data loop: max(1, size / 2).
        uint32_t AppendMipChain(std::vector<uint8_t>& pixels, uint32_t width, uint32_t height)
        {
            uint32_t levels = 1;
            size_t prevOffset = 0;
            uint32_t pw = width, ph = height;

            while (pw > 1 || ph > 1)
            {
                const uint32_t nw = std::max(1u, pw / 2);
                const uint32_t nh = std::max(1u, ph / 2);

                const size_t newOffset = pixels.size();
                pixels.resize(newOffset + static_cast<size_t>(nw) * nh * 4);   // may reallocate: index, never hold pointers

                for (uint32_t y = 0; y < nh; ++y)
                {
                    const uint32_t y0 = std::min(2 * y,     ph - 1);
                    const uint32_t y1 = std::min(2 * y + 1, ph - 1);
                    for (uint32_t x = 0; x < nw; ++x)
                    {
                        const uint32_t x0 = std::min(2 * x,     pw - 1);
                        const uint32_t x1 = std::min(2 * x + 1, pw - 1);

                        for (uint32_t c = 0; c < 4; ++c)
                        {
                            auto at = [&](uint32_t sx, uint32_t sy) -> uint32_t {
                                return pixels[prevOffset + (static_cast<size_t>(sy) * pw + sx) * 4 + c];
                            };
                            const uint32_t sum = at(x0, y0) + at(x1, y0) + at(x0, y1) + at(x1, y1);
                            pixels[newOffset + (static_cast<size_t>(y) * nw + x) * 4 + c] = static_cast<uint8_t>((sum + 2) / 4);
                        }
                    }
                }

                prevOffset = newOffset;
                pw = nw;
                ph = nh;
                ++levels;
            }
            return levels;
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
                                                       const std::string& debugName, bool generateMipChain)
        {
            RHI::TextureDesc desc;
            desc.type      = RHI::TextureType::Texture2D;
            desc.format    = RHI::TextureFormat::RGBA8_UNORM;
            desc.width     = width;
            desc.height    = height;
            desc.mipLevels = generateMipChain ? AppendMipChain(pixels, width, height) : 1;
            desc.initialData = pixels.data();    // complete chain, contiguous: exactly what D3D11Texture expects
            desc.debugName = debugName;
            return CreateTexture(device, desc);
        }
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::LoadTexture2D(RHI::IGraphicsDevice& device,
                                                              const std::string& path,
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
        return CreateFromRGBA8(device, pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), path, generateMipChain);
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::LoadTexture2DFromMemory(RHI::IGraphicsDevice& device,
                                                                        const uint8_t* data, size_t sizeBytes,
                                                                        const std::string& debugName,
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
        return CreateFromRGBA8(device, pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), debugName, generateMipChain);
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::CreateTexture2DFromRGBA8(RHI::IGraphicsDevice& device,
                                                                         const uint8_t* rgba,
                                                                         uint32_t width, uint32_t height,
                                                                         const std::string& debugName,
                                                                         bool generateMipChain)
    {
        if (!rgba || width == 0 || height == 0)
        {
            Log("invalid pixel data for '" + debugName + "'");
            return nullptr;
        }
        std::vector<uint8_t> pixels(rgba, rgba + static_cast<size_t>(width) * height * 4);
        return CreateFromRGBA8(device, pixels, width, height, debugName, generateMipChain);
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::LoadCubeMap(RHI::IGraphicsDevice& device,
                                                            const std::array<std::string, 6>& facePaths)
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
        desc.format      = RHI::TextureFormat::RGBA8_UNORM;
        desc.width       = static_cast<uint32_t>(width);
        desc.height      = static_cast<uint32_t>(height);
        desc.mipLevels   = 1;
        desc.arraySize   = 1;                 // the RHI multiplies by 6 for cube maps
        desc.initialData = pixels.data();     // six faces back to back, face i = array slice i
        desc.debugName   = facePaths[0] + " (+5 faces)";

        return CreateTexture(device, desc);
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::CreateSolidColor(RHI::IGraphicsDevice& device,
                                                                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        const uint8_t pixel[4] = { r, g, b, a };

        RHI::TextureDesc desc;
        desc.type        = RHI::TextureType::Texture2D;
        desc.format      = RHI::TextureFormat::RGBA8_UNORM;
        desc.width       = 1;
        desc.height      = 1;
        desc.mipLevels   = 1;
        desc.initialData = pixel;
        desc.debugName   = "SolidColor";

        return CreateTexture(device, desc);
    }
}
