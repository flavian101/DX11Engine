#include "dxpch.h"
#include "TextureLoader.h"
#include "utils/Texture.h"
#include "ModelLoaderUtils.h"

namespace DXEngine
{
    std::shared_ptr<Texture> TextureLoader::LoadTexture(const std::string& filepath, aiTextureType type)
    {
        if (filepath.empty())
        {
            OutputDebugStringA("Filepath is empty");
            return CreateSolidColorTexture(255, 255, 255, 255);
        }

        //check cache first
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

        //check if texture is embedded(starts with'*')
        if (filepath[0] == '*')
        {
            OutputDebugStringA("TextureLoader: Embedded texture detected but no scene provided\n");
            return CreateFallbackTexture(type);
        }

        //load from file
        if (!ModelLoaderUtils::FileExists(filepath))
        {
            OutputDebugStringA(("TextureLoader: Texture file not found: " + filepath + "\n").c_str());
            return CreateFallbackTexture(type);
        }
        try
        {
            auto texture = Texture::CreateFromFile(filepath);
            if (texture && texture->IsValid())
            {
                //cache the texture
                if (m_CachingEnabled)
                {
                    m_TextureCache[filepath] = texture;
                }
                m_TexturesLoaded++;
                return texture;
            }
        }
        catch (const std::exception& e)
        {
            OutputDebugStringA(("TextureLoader: Failed to load texture: " +
                std::string(e.what()) + "\n").c_str());
        }

        return CreateFallbackTexture(type);
    }

    std::shared_ptr<Texture> TextureLoader::LoadEmbeddedTexture(const aiScene* scene, const std::string& filepath)
    {
        if (!scene || filepath.empty() || filepath[0] != '*')
        {
            OutputDebugStringA("TexureLoader: Error (either scene, filepath or embeded path empty)");
            return CreateSolidColorTexture(255, 255, 255, 255);
        }

        int textureIndex = std::stoi(filepath.substr(1));

        if (textureIndex >= 0 && textureIndex < static_cast<int>(scene->mNumTextures))
        {
            const aiTexture* texture = scene->mTextures[textureIndex];

            //Handele compresses texture (png jpg, etc)
            if (texture->mHeight == 0)
            {
                const unsigned char* data = reinterpret_cast<const unsigned char*>(texture->pcData);
                size_t dataSize = texture->mWidth;

                auto loadedTexture = Texture::CreateFromMemory(data, dataSize);
                if (loadedTexture && loadedTexture->IsValid())
                {
                    m_TexturesLoaded++;
                    return loadedTexture;
                }
            }
            else
            {
                //uncompressed texture data (RGBA)
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

                auto loadedTexture = Texture::CreateFromPixels(pixels.data(), static_cast<int>(width), static_cast<int>(height), 4);

                if (loadedTexture && loadedTexture->IsValid())
                {
                    m_TexturesLoaded++;
                    return loadedTexture;
                }
            }
        }
        return CreateSolidColorTexture(255, 255, 255, 255);
    }

    std::shared_ptr<Texture> TextureLoader::CreateFallbackTexture(aiTextureType type)
    {
        switch (type)
        {
        case aiTextureType_DIFFUSE:
            return CreateSolidColorTexture(255, 255, 255, 255);

        case aiTextureType_NORMALS:
            return CreateSolidColorTexture(128, 128, 255, 128);

        case aiTextureType_SPECULAR:
            return CreateSolidColorTexture(0, 0, 0, 255);

        case aiTextureType_METALNESS:
            return CreateSolidColorTexture(128, 128, 128, 255);

        case aiTextureType_DIFFUSE_ROUGHNESS:
            return CreateSolidColorTexture(128, 128, 128, 255);

        case aiTextureType_AMBIENT_OCCLUSION:
            return CreateSolidColorTexture(255, 255, 255, 255);

        case aiTextureType_HEIGHT:
            return CreateSolidColorTexture(0, 0, 0, 255);

        case aiTextureType_EMISSIVE:
            return CreateSolidColorTexture(0, 0, 0, 255);

        case aiTextureType_OPACITY:
            return CreateSolidColorTexture(255, 255, 255, 255);
            
        default:
            CreateSolidColorTexture(255, 0, 255, 255);
        }
        return std::shared_ptr<Texture>();
    }

    std::shared_ptr<Texture> TextureLoader::CreateSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        uint8_t pixels[4] = { r,g,b,a };
        return Texture::CreateFromPixels(pixels,1,1,4);
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

    bool TextureLoader::IsHeightMap(std::shared_ptr<Texture> texture) const
    {
        if (!texture)
        {
            OutputDebugStringA("No Texture to check if height map");
            return false;
        }
        const std::string& filename = texture->GetFilePath();

        TextureType type = TextureUtils::DetectTextureType(filename);
        return (type == TextureType::Height);
    }
}
