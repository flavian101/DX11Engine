#pragma once
#include <memory>
#include <assimp/scene.h>
#include <assimp/material.h>
#include "ModelLoaderUtils.h"
#include "utils/material/MaterialTypes.h"   // TextureSlot (its underlying type must match: no forward declaration)
#include <functional>
#include <string>
#include <vector>

namespace DXEngine
{ 
	class Material;
	class TextureLoader;

	class MaterialProcessor
	{
	public:
		MaterialProcessor(std::shared_ptr<TextureLoader> textureLoader);

		// 'warnings' (optional) collects what was left out and why: a map that is not used, a file that would not load.
		std::shared_ptr<Material> ProcessMaterial(
			const aiMaterial* aiMaterial,
			const std::string& directory,
			const ModelLoadOptions& options,
			std::vector<std::string>* warnings = nullptr);

		// WHAT the pixels compute and HOW the surface is drawn are decided separately (see MaterialTypes.h): an imported
		// material with opacity < 1 is a Lit material with a blended RenderState, not a different kind of material.
		ShadingModel DetermineShadingModel(const aiMaterial* material) const;
		RenderState  DetermineRenderState(const aiMaterial* material) const;

		size_t GetMaterialsProcessed() const { return m_MaterialsProcessed; }
		void ResetStats() { m_MaterialsProcessed = 0; }

	private:
		// What the file itself said about the factors (it matters for how a map is combined with them: see MaterialMapPlan.h)
		struct FileFactors { bool metallic = false; bool roughness = false; };

		FileFactors LoadBasicProperties(std::shared_ptr<Material> mat, const aiMaterial* aiMat);

		// Collects the material's texture files, lets MaterialMapPlan decide which feeds which slot and channel, loads them.
		void LoadAllTextures(
			std::shared_ptr<Material>mat,
			const aiMaterial* aiMat, 
			const std::string& directory, 
			const ModelLoadOptions& options,
			const FileFactors& fileFactors,
			std::vector<std::string>* warnings);

		// Loads the texture file (as the model names it) into `slot`. Returns true when it is bound.
		bool LoadTextureFile(std::shared_ptr<Material> mat, const std::string& relativePath,
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
