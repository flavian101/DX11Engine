#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <assimp/scene.h>

namespace DXEngine
{
	class Texture;

	class TextureLoader
	{
	public:
		TextureLoader() = default;

		std::shared_ptr<Texture> LoadTexture(const std::string& filepath, aiTextureType type);
		std::shared_ptr<Texture> LoadEmbeddedTexture(const aiScene* scene, const std::string& filepath);

		std::shared_ptr<Texture> CreateFallbackTexture(aiTextureType type);
		std::shared_ptr<Texture> CreateSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

		// Texture type utilities
		std::string GetTextureTypeName(aiTextureType type) const;
		bool IsHeightMap(std::shared_ptr<Texture> texture) const;

		// Cache management
		void EnableCaching(bool enable) { m_CachingEnabled = enable; }
		void ClearCache() { m_TextureCache.clear(); }
		size_t GetCacheSize() const { return m_TextureCache.size(); }

		// Statistics
		size_t GetTexturesLoaded() const { return m_TexturesLoaded; }
		void ResetStats() { m_TexturesLoaded = 0; }


	private:
		std::unordered_map<std::string, std::weak_ptr<Texture>> m_TextureCache;
		bool m_CachingEnabled = true;
		size_t m_TexturesLoaded = 0;
	};
}
