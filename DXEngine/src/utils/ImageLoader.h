#pragma once
#include "RHI/GraphicsDevice.h"
#include "material/MaterialSlots.h"   // TextureColorSpace, TextureSlot
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace DXEngine
{
    // ==================================================================
    // ImageLoader: image file -> RHI texture.
    //
    // (Named ImageLoader because models/processors/TextureLoader already exists for Assimp.)
    // Replaces the legacy Texture / CubeMapTexture classes, which talked to raw D3D11 through
    // the deleted RenderCommand. Pixels are always decoded to RGBA8 and returned as RHI::ITexture so
    // Material::SetTexture accepts them.
    //
    // COLOUR SPACE is a required argument, on purpose. A colour texture (albedo, emissive, a sky) must be RGBA8_SRGB so the
    // GPU converts it to linear before any lighting maths; a data texture (normals, roughness, AO, ...) must be
    // RGBA8_UNORM because its numbers are not colours. Getting it wrong does not crash: it just looks subtly off, which is
    // why there is no default. LoadForSlot / LoadCubeMapForSlot take it from the slot table (MaterialSlots.cpp).
    //
    // Failures are logged and return nullptr; a material simply renders without that texture.
    // ==================================================================
    class ImageLoader
    {
    public:
        // generateMipChain: build the full mip chain on the CPU (2x2 box filter).
        // The legacy loader created a single level, which shimmers badly on tiled ground.
        // (For sRGB images the chain is filtered in linear light: see ImageMips.h.)
        static std::shared_ptr<RHI::ITexture> LoadTexture2D(RHI::IGraphicsDevice& device,
                                                            const std::string& path,
                                                            TextureColorSpace colorSpace,
                                                            bool generateMipChain = true);

        // The colour space comes from the slot the texture is meant for: Diffuse -> sRGB, Normal -> linear, ...
        static std::shared_ptr<RHI::ITexture> LoadForSlot(RHI::IGraphicsDevice& device, const std::string& path,
                                                          TextureSlot slot, bool generateMipChain = true);

        // Decode an image that is already in memory (for example a PNG/JPG embedded in a glTF/FBX).
        static std::shared_ptr<RHI::ITexture> LoadTexture2DFromMemory(RHI::IGraphicsDevice& device,
                                                                      const uint8_t* data, size_t sizeBytes,
                                                                      const std::string& debugName,
                                                                      TextureColorSpace colorSpace,
                                                                      bool generateMipChain = true);

        // Raw RGBA8 pixels (width * height * 4 bytes), for example an uncompressed embedded texture.
        static std::shared_ptr<RHI::ITexture> CreateTexture2DFromRGBA8(RHI::IGraphicsDevice& device,
                                                                       const uint8_t* rgba,
                                                                       uint32_t width, uint32_t height,
                                                                       const std::string& debugName,
                                                                       TextureColorSpace colorSpace,
                                                                       bool generateMipChain = true);

        // Face i of the array comes from facePaths[i] (same order as the legacy CubeMapTexture).
        // All six images must have the same size.
        static std::shared_ptr<RHI::ITexture> LoadCubeMap(RHI::IGraphicsDevice& device,
                                                          const std::array<std::string, 6>& facePaths,
                                                          TextureColorSpace colorSpace);
        static std::shared_ptr<RHI::ITexture> LoadCubeMapForSlot(RHI::IGraphicsDevice& device,
                                                                 const std::array<std::string, 6>& facePaths, TextureSlot slot);

        // 1x1 texture, handy as a neutral fallback (white diffuse, flat normal, ...).
        static std::shared_ptr<RHI::ITexture> CreateSolidColor(RHI::IGraphicsDevice& device,
                                                               uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                                               TextureColorSpace colorSpace);
    };
}
