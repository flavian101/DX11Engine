#pragma once
#include <memory>
#include <assimp/scene.h>
#include <assimp/material.h>
#include "ModelLoaderUtils.h"
#include "utils/material/MaterialTypes.h"   // TextureSlot (its underlying type must match: no forward declaration)
#include <functional>

namespace DXEngine
{ 
	class Material;
	class TextureLoader;

	class MaterialProcessor
	{
	public:
		MaterialProcessor(std::shared_ptr<TextureLoader> textureLoader);

		std::shared_ptr<Material> ProcessMaterial(
			const aiMaterial* aiMaterial,
			const std::string& directory,
			const ModelLoadOptions& options);

		// WHAT the pixels compute and HOW the surface is drawn are decided separately (see MaterialTypes.h): an imported
		// material with opacity < 1 is a Lit material with a blended RenderState, not a different kind of material.
		ShadingModel DetermineShadingModel(const aiMaterial* material) const;
		RenderState  DetermineRenderState(const aiMaterial* material) const;

		size_t GetMaterialsProcessed() const { return m_MaterialsProcessed; }
		void ResetStats() { m_MaterialsProcessed = 0; }

	private:
		void LoadBasicProperties(std::shared_ptr<Material> mat, const aiMaterial* aiMat);
		void LoadAllTextures(
			std::shared_ptr<Material>mat,
			const aiMaterial* aiMat, 
			const std::string& directory, 
			const ModelLoadOptions& options);
		// Loads the texture of `type` (if the model has one) into `slot`.
		// Returns the resolved file path, or "" when nothing was loaded.
		std::string LoadTextureOfType(std::shared_ptr<Material> mat, const aiMaterial* aiMat,
			const std::string& directory, aiTextureType type, TextureSlot slot);

		void ConfigureMaterialFromTextures(std::shared_ptr<Material> mat);

		std::string GetTextureFilename(const aiMaterial* material, aiTextureType type,
			const std::string& directory);

	private:
		std::string ResolveTexturePath(const std::string& texturePath, const std::string& modelDirectory);


	private:
		std::shared_ptr<TextureLoader> m_TextureLoader;
		size_t m_MaterialsProcessed = 0;
	};
}
