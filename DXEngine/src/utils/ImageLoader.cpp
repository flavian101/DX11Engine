#include "dxpch.h"
#include "ImageLoader.h"
#include "utils/AssetNames.h"
#include "ImageMips.h"

#include "ImageDecode.h"       // the decoding (stb_image, Radiance HDR, OpenEXR) lives there, away from the GPU

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
        ImageDecode::Image image = ImageDecode::DecodeFile(path, colorSpace);
        if (!image.ok())
        {
            Log(image.error);
            return nullptr;
        }
        return CreateFromRGBA8(device, image.rgba, static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), path, colorSpace, generateMipChain);
    }

    std::shared_ptr<RHI::ITexture> ImageLoader::LoadTexture2DFromMemory(RHI::IGraphicsDevice& device,
                                                                        const uint8_t* data, size_t sizeBytes,
                                                                        const std::string& debugName,
                                                                        TextureColorSpace colorSpace,
                                                                        bool generateMipChain)
    {
        ImageDecode::Image image = ImageDecode::DecodeMemory(data, sizeBytes, debugName, colorSpace);
        if (!image.ok())
        {
            Log(image.error);
            return nullptr;
        }
        return CreateFromRGBA8(device, image.rgba, static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), debugName, colorSpace, generateMipChain);
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
            ImageDecode::Image face = ImageDecode::DecodeFile(facePaths[i], colorSpace);
            if (!face.ok())
            {
                Log("cube face: " + face.error);
                return nullptr;
            }
            if (i == 0) { width = face.width; height = face.height; pixels.reserve(static_cast<size_t>(width) * height * 4 * 6); }
            if (face.width != width || face.height != height)
            {
                Log("cube face '" + facePaths[i] + "' has a different size than the first face");
                return nullptr;
            }
            pixels.insert(pixels.end(), face.rgba.begin(), face.rgba.end());
        }

        RHI::TextureDesc desc;
        desc.type        = RHI::TextureType::TextureCube;
        desc.format      = FormatFor(colorSpace);
        desc.width       = static_cast<uint32_t>(width);
        desc.height      = static_cast<uint32_t>(height);
        desc.mipLevels   = 1;
        desc.arraySize   = 1;                 // the RHI multiplies by 6 for cube maps
        desc.initialData = pixels.data();     // six faces back to back, face i = array slice i
        desc.debugName   = AssetNames::CubeMapName(facePaths);        // "cube:a|b|c|d|e|f": a scene file can load it again

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
