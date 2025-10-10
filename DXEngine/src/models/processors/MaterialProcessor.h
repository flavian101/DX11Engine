#pragma once
#include <memory>
#include <assimp/scene.h>
#include <assimp/material.h>
#include "ModelLoaderUtils.h"
#include <functional>

namespace DXEngine
{ 
	class Material;
	enum class MaterialType;
	class TextureLoader;
	class Texture;

	class MaterialProcessor
	{
	public:
		MaterialProcessor(std::shared_ptr<TextureLoader> textureLoader);

		std::shared_ptr<Material> ProcessMaterial(
			const aiMaterial* aiMaterial,
			const std::string& directory,
			const ModelLoadOptions& options);

		MaterialType DetermineMaterialType(const aiMaterial* material) const;

		size_t GetMaterialsProcessed() const { return m_MaterialsProcessed; }
		void ResetStats() { m_MaterialsProcessed = 0; }

	private:
		void LoadBasicProperties(std::shared_ptr<Material> mat, const aiMaterial* aiMat);
		void LoadAllTextures(
			std::shared_ptr<Material>mat,
			const aiMaterial* aiMat, 
			const std::string& directory, 
			const ModelLoadOptions& options);
		void LoadTextureOfType(std::shared_ptr<Material> mat, const aiMaterial* aiMat,
			const std::string& directory, aiTextureType type,
			std::function<void(std::shared_ptr<Texture>)> setter);

		void ConfigureMaterialFromTextures(std::shared_ptr<Material> mat);

		std::string GetTextureFilename(const aiMaterial* material, aiTextureType type,
			const std::string& directory);


	private:
		std::shared_ptr<TextureLoader> m_TextureLoader;
		size_t m_MaterialsProcessed = 0;
	};
}
