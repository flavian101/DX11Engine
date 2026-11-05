#pragma once
#include "RHI/GraphicsDevice.h"
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
    // the deleted RenderCommand. Pixels are always decoded to RGBA8 (RGBA8_UNORM), like the
    // old loaders, and returned as RHI::ITexture so Material::SetTexture accepts them.
    //
    // Failures are logged and return nullptr; a material simply renders without that texture.
    // ==================================================================
    class ImageLoader
    {
    public:
        // generateMipChain: build the full mip chain on the CPU (2x2 box filter).
        // The legacy loader created a single level, which shimmers badly on tiled ground.
        static std::shared_ptr<RHI::ITexture> LoadTexture2D(RHI::IGraphicsDevice& device,
                                                            const std::string& path,
                                                            bool generateMipChain = true);

        // Decode an image that is already in memory (for example a PNG/JPG embedded in a glTF/FBX).
        static std::shared_ptr<RHI::ITexture> LoadTexture2DFromMemory(RHI::IGraphicsDevice& device,
                                                                      const uint8_t* data, size_t sizeBytes,
                                                                      const std::string& debugName,
                                                                      bool generateMipChain = true);

        // Raw RGBA8 pixels (width * height * 4 bytes), for example an uncompressed embedded texture.
        static std::shared_ptr<RHI::ITexture> CreateTexture2DFromRGBA8(RHI::IGraphicsDevice& device,
                                                                       const uint8_t* rgba,
                                                                       uint32_t width, uint32_t height,
                                                                       const std::string& debugName,
                                                                       bool generateMipChain = true);

        // Face i of the array comes from facePaths[i] (same order as the legacy CubeMapTexture).
        // All six images must have the same size.
        static std::shared_ptr<RHI::ITexture> LoadCubeMap(RHI::IGraphicsDevice& device,
                                                          const std::array<std::string, 6>& facePaths);

        // 1x1 texture, handy as a neutral fallback (white diffuse, flat normal, ...).
        static std::shared_ptr<RHI::ITexture> CreateSolidColor(RHI::IGraphicsDevice& device,
                                                               uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    };
}
