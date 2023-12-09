#pragma once

#include "Graphics.h"
#include <unordered_map>
#include "wrl.h"
#include <string>


class Texture
{
public:
    Texture(Graphics& g, const char* filename, UINT slot);

    void Bind(Graphics& g);


    // Function to get the shader resource view
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetShaderResourceView() const {
        return textureView;
    }
private:
    void LoadTexture(Graphics& g, const char* filename);
    UINT slot;
    const char* filename;
    Microsoft::WRL::ComPtr< ID3D11ShaderResourceView> textureView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> imageTexture;
};





class TextureCache
{
public:
    TextureCache(Graphics& g)
        : graphics(g) {}

	~TextureCache()
	{
		ClearCache();
	}
    // Function to get a texture from the cache or load it if not present
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTexture(const std::string& filename, UINT slot = 0) {
        auto it = textureMap.find(filename);

        if (it != textureMap.end()) {
            // Texture found in the cache
            return it->second;
        }
        else {
            // Texture not in the cache, load and cache it
            auto texture = std::make_shared<Texture>(graphics, filename.c_str(), slot);
            textureMap[filename] = texture->GetShaderResourceView();
            return textureMap[filename];
        }
    }

    // Clear the texture cache
    void ClearCache() {
        textureMap.clear();
    }

private:
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textureMap;
    Graphics& graphics;

};
