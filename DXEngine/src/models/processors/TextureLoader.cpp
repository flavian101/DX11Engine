#include "dxpch.h"
#include "TextureLoader.h"
#include "RHI/GraphicsDevice.h"
#include "utils/ImageLoader.h"
#include "utils/TextureNaming.h"
#include "ModelLoaderUtils.h"
#include <filesystem>

namespace DXEngine
{
    TextureLoader::TextureLoader(std::shared_ptr<RHI::IGraphicsDevice> device) : m_Device(std::move(device)) {}

    std::shared_ptr<RHI::ITexture> TextureLoader::LoadTexture(const std::string& filepath, aiTextureType type)
    {
        if (filepath.empty())
        {
            OutputDebugStringA("TextureLoader: Filepath is empty\n");
            return CreateFallbackTexture(type);
        }

        // Check if texture is embedded (starts with '*')
        if (filepath[0] == '*')
        {
            if (!m_CurrentScene)
            {
                OutputDebugStringA("TextureLoader: Embedded texture detected but no scene provided\n");
                return CreateFallbackTexture(type);
            }

            // Not cached by name: "*0" means a different image in every model.
            auto embedded = LoadEmbeddedTexture(m_CurrentScene, filepath);
            return embedded ? embedded : CreateFallbackTexture(type);
        }

        // Check cache first
        if (m_CachingEnabled)
        {
            auto it = m_TextureCache.find(filepath);
            if (it != m_TextureCache.end())
            {
                if (auto cached = it->second.lock())
                {
                    return cached;
                }
                else
                {
                    m_TextureCache.erase(it);
                }
            }
        }

        // Check if file exists
        if (!ModelLoaderUtils::FileExists(filepath))
        {
            OutputDebugStringA(("TextureLoader: Texture file not found: " + filepath + "\n").c_str());
            return CreateFallbackTexture(type);
        }

        // Load from file (ImageLoader logs the reason and returns nullptr on failure)
        if (m_Device)
        {
            if (auto texture = ImageLoader::LoadTexture2D(*m_Device, filepath))
            {
                if (m_CachingEnabled)
                {
                    m_TextureCache[filepath] = texture;
                }
                m_TexturesLoaded++;
                return texture;
            }
        }

        return CreateFallbackTexture(type);
    }

    std::shared_ptr<RHI::ITexture> TextureLoader::LoadEmbeddedTexture(const aiScene* scene, const std::string& filepath)
    {
        if (!scene || filepath.empty() || filepath[0] != '*')
        {
            OutputDebugStringA("TextureLoader: Invalid embedded texture path\n");
            return CreateSolidColorTexture(255, 255, 255, 255);
        }

        int textureIndex = std::stoi(filepath.substr(1));

        if (textureIndex >= 0 && textureIndex < static_cast<int>(scene->mNumTextures))
        {
            const aiTexture* texture = scene->mTextures[textureIndex];

            // Handle compressed texture (png, jpg, etc)
            if (texture->mHeight == 0)
            {
                const unsigned char* data = reinterpret_cast<const unsigned char*>(texture->pcData);
                size_t dataSize = texture->mWidth;

                auto loadedTexture = m_Device ? ImageLoader::LoadTexture2DFromMemory(*m_Device, data, dataSize, filepath) : nullptr;
                if (loadedTexture)
                {
                    m_TexturesLoaded++;
#ifdef DX_DEBUG
                    OutputDebugStringA(("TextureLoader: Loaded embedded compressed texture\n"));
#endif
                    return loadedTexture;
                }
            }
            else
            {
                // Uncompressed texture data (RGBA)
                const aiTexel* texels = texture->pcData;
                size_t width = texture->mWidth;
                size_t height = texture->mHeight;

                std::vector<unsigned char> pixels(width * height * 4);
                for (size_t i = 0; i < width * height; i++)
                {
                    pixels[i * 4 + 0] = texels[i].r;
                    pixels[i * 4 + 1] = texels[i].g;
                    pixels[i * 4 + 2] = texels[i].b;
                    pixels[i * 4 + 3] = texels[i].a;
                }

                auto loadedTexture = m_Device ? ImageLoader::CreateTexture2DFromRGBA8(*m_Device, pixels.data(),
                    static_cast<uint32_t>(width), static_cast<uint32_t>(height), filepath) : nullptr;

                if (loadedTexture)
                {
                    m_TexturesLoaded++;
#ifdef DX_DEBUG
                    OutputDebugStringA(("TextureLoader: Loaded embedded uncompressed texture\n"));
#endif
                    return loadedTexture;
                }
            }
        }

        OutputDebugStringA(("TextureLoader: Failed to load embedded texture at index " +
            std::to_string(textureIndex) + "\n").c_str());
        return CreateSolidColorTexture(255, 255, 255, 255);
    }

    std::shared_ptr<RHI::ITexture> TextureLoader::CreateFallbackTexture(aiTextureType type)
    {
#ifdef DX_DEBUG
        OutputDebugStringA(("TextureLoader: Creating fallback texture for type: " +
            GetTextureTypeName(type) + "\n").c_str());
#endif

        switch (type)
        {
        case aiTextureType_DIFFUSE:
            return CreateSolidColorTexture(255, 255, 255, 255); // White

        case aiTextureType_NORMALS:
            return CreateSolidColorTexture(128, 128, 255, 255); // Flat normal (pointing up)

        case aiTextureType_SPECULAR:
            return CreateSolidColorTexture(128, 128, 128, 255); // Medium gray

        case aiTextureType_METALNESS:
            return CreateSolidColorTexture(0, 0, 0, 255); // Non-metallic

        case aiTextureType_DIFFUSE_ROUGHNESS:
            return CreateSolidColorTexture(128, 128, 128, 255); // Medium roughness

        case aiTextureType_AMBIENT_OCCLUSION:
            return CreateSolidColorTexture(255, 255, 255, 255); // No occlusion

        case aiTextureType_HEIGHT:
            return CreateSolidColorTexture(128, 128, 128, 255); // Flat height

        case aiTextureType_EMISSIVE:
            return CreateSolidColorTexture(0, 0, 0, 255); // No emission

        case aiTextureType_OPACITY:
            return CreateSolidColorTexture(255, 255, 255, 255); // Fully opaque

        default:
            return CreateSolidColorTexture(255, 0, 255, 255); // Magenta (error color)
        }
    }

    std::shared_ptr<RHI::ITexture> TextureLoader::CreateSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        if (!m_Device) return nullptr;

        const uint32_t key = (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | uint32_t(a);
        auto it = m_SolidColorCache.find(key);
        if (it != m_SolidColorCache.end()) return it->second;

        auto texture = ImageLoader::CreateSolidColor(*m_Device, r, g, b, a);
        if (texture) m_SolidColorCache[key] = texture;
        return texture;
    }

    std::string TextureLoader::GetTextureTypeName(aiTextureType type) const
    {
        switch (type) {
        case aiTextureType_DIFFUSE: return "Diffuse";
        case aiTextureType_NORMALS: return "Normal";
        case aiTextureType_SPECULAR: return "Specular";
        case aiTextureType_EMISSIVE: return "Emissive";
        case aiTextureType_METALNESS: return "Metallic";
        case aiTextureType_DIFFUSE_ROUGHNESS: return "Roughness";
        case aiTextureType_AMBIENT_OCCLUSION: return "AO";
        case aiTextureType_HEIGHT: return "Height";
        case aiTextureType_DISPLACEMENT: return "Displacement";
        case aiTextureType_OPACITY: return "Opacity";
        case aiTextureType_REFLECTION: return "Reflection";
        default: return "Unknown";
        }
    }

    bool TextureLoader::IsHeightMap(const std::string& filepath) const
    {
        if (filepath.empty())
            return false;

        return TextureUtils::DetectTextureType(filepath) == TextureType::Height;
    }
}