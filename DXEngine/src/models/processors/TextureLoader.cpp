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

    namespace
    {
        // A model that names "x_nor_gl.exr" often has "x_nor_gl.png" next to it (an EXR the engine cannot decode, converted):
        // the same stem with another image extension, in the same folder.
        std::string FindAlternateImage(const std::string& filepath)
        {
            namespace fs = std::filesystem;
            std::error_code ec;
            const fs::path original(filepath);
            for (const char* extension : { ".png", ".jpg", ".jpeg", ".tga", ".bmp" })
            {
                fs::path candidate = original;
                candidate.replace_extension(extension);
                if (candidate != original && fs::is_regular_file(candidate, ec))
                    return candidate.string();
            }
            return {};
        }
    }

    // Returns nullptr when the image cannot be had: NO substitute colour. A material with a missing map must be a material
    // WITHOUT that map (it then uses its factor), not one with a white or grey stand-in that looks bound: a white "metallic"
    // map made a whole model metal, a white "normal" map tilted every normal. The caller says what was missing.
    std::shared_ptr<RHI::ITexture> TextureLoader::LoadTexture(const std::string& filepath, aiTextureType type, TextureColorSpace colorSpace)
    {
        if (filepath.empty())
        {
            OutputDebugStringA("TextureLoader: Filepath is empty\n");
            return nullptr;
        }

        // Check if texture is embedded (starts with '*')
        if (filepath[0] == '*')
        {
            if (!m_CurrentScene)
            {
                OutputDebugStringA("TextureLoader: Embedded texture detected but no scene provided\n");
                return nullptr;
            }

            // Not cached by name: "*0" means a different image in every model.
            return LoadEmbeddedTexture(m_CurrentScene, filepath, colorSpace);
        }

        // Check cache first. The colour space is part of the key (see LoadTexture in the header).
        const std::string cacheKey = filepath + (colorSpace == TextureColorSpace::SRGB ? "|srgb" : "|linear");
        if (m_CachingEnabled)
        {
            auto it = m_TextureCache.find(cacheKey);
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

        if (!m_Device)
            return nullptr;

        // the file itself, then (when it is missing or cannot be decoded) a converted copy next to it
        std::shared_ptr<RHI::ITexture> texture;
        if (ModelLoaderUtils::FileExists(filepath))
            texture = ImageLoader::LoadTexture2D(*m_Device, filepath, colorSpace);      // logs the reason on failure
        else
            OutputDebugStringA(("TextureLoader: Texture file not found: " + filepath + "\n").c_str());

        if (!texture)
        {
            const std::string alternate = FindAlternateImage(filepath);
            if (!alternate.empty())
            {
                texture = ImageLoader::LoadTexture2D(*m_Device, alternate, colorSpace);
                if (texture)
                    OutputDebugStringA(("TextureLoader: using '" + alternate + "' instead of '" + filepath + "'\n").c_str());
            }
        }

        if (!texture)
            return nullptr;

        if (m_CachingEnabled)
            m_TextureCache[cacheKey] = texture;
        m_TexturesLoaded++;
        return texture;
    }

    std::shared_ptr<RHI::ITexture> TextureLoader::LoadEmbeddedTexture(const aiScene* scene, const std::string& filepath, TextureColorSpace colorSpace)
    {
        if (!scene || filepath.empty() || filepath[0] != '*')
        {
            OutputDebugStringA("TextureLoader: Invalid embedded texture path\n");
            return nullptr;
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

                auto loadedTexture = m_Device ? ImageLoader::LoadTexture2DFromMemory(*m_Device, data, dataSize, filepath, colorSpace) : nullptr;
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
                    static_cast<uint32_t>(width), static_cast<uint32_t>(height), filepath, colorSpace) : nullptr;

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
        return nullptr;
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