#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <assimp/scene.h>

namespace DXEngine
{
	namespace RHI { class IGraphicsDevice; class ITexture; }

	// Loads the textures a model refers to (files, embedded images) as RHI textures,
	// with a per-path cache and neutral fallbacks for missing files.
	class TextureLoader
	{
	public:
		explicit TextureLoader(std::shared_ptr<RHI::IGraphicsDevice> device);

		std::shared_ptr<RHI::ITexture> LoadTexture(const std::string& filepath, aiTextureType type);
		std::shared_ptr<RHI::ITexture> LoadEmbeddedTexture(const aiScene* scene, const std::string& filepath);

		std::shared_ptr<RHI::ITexture> CreateFallbackTexture(aiTextureType type);
		std::shared_ptr<RHI::ITexture> CreateSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

		// Texture type utilities
		std::string GetTextureTypeName(aiTextureType type) const;
		// Decided from the FILE NAME (an RHI texture does not remember where it came from).
		bool IsHeightMap(const std::string& filepath) const;

		// Cache management
		void EnableCaching(bool enable) { m_CachingEnabled = enable; }
		void ClearCache() { m_TextureCache.clear(); m_SolidColorCache.clear(); }
		size_t GetCacheSize() const { return m_TextureCache.size(); }

		// Statistics
		size_t GetTexturesLoaded() const { return m_TexturesLoaded; }
		void ResetStats() { m_TexturesLoaded = 0; }

	private:
		std::shared_ptr<RHI::IGraphicsDevice> m_Device;
		std::unordered_map<std::string, std::weak_ptr<RHI::ITexture>> m_TextureCache;
		std::unordered_map<uint32_t, std::shared_ptr<RHI::ITexture>>  m_SolidColorCache;   // one 1x1 per colour, not one per request
		bool   m_CachingEnabled = true;
		size_t m_TexturesLoaded = 0;
	};
}
