#include "dxpch.h"
#include "MaterialProcessor.h"
#include "utils/material/Material.h"
#include "TextureLoader.h"
#include "RHI/GraphicsDevice.h"
#include "renderer/Renderer.h"
#include <algorithm>
#include <filesystem>

namespace DXEngine
{
	MaterialProcessor::MaterialProcessor(std::shared_ptr<TextureLoader> textureLoader)
		: m_TextureLoader(textureLoader), m_MaterialsProcessed(0)
	{
		if (!m_TextureLoader)
		{
			m_TextureLoader = std::make_shared<TextureLoader>(Renderer::GetDevice());
		}
	}

	std::shared_ptr<Material> MaterialProcessor::ProcessMaterial(
		const aiMaterial* aiMaterial,
		const std::string& directory,
		const ModelLoadOptions& options)
	{
		if (!aiMaterial)
		{
			OutputDebugStringA("MaterialProcessor: Null aiMaterial\n");
			return nullptr;
		}

		// Get Material name
		aiString materialName;
		aiMaterial->Get(AI_MATKEY_NAME, materialName);
		std::string name = materialName.C_Str();
		if (name.empty())
			name = "Material_" + std::to_string(m_MaterialsProcessed);

		const ShadingModel model = DetermineShadingModel(aiMaterial);
		auto material = std::make_shared<Material>(name, model);
		material->SetRenderState(DetermineRenderState(aiMaterial));

		LoadBasicProperties(material, aiMaterial);

		if (options.loadTextures)
		{
			LoadAllTextures(material, aiMaterial, directory, options);
		}

		// Configure Material based on loaded textures
		ConfigureMaterialFromTextures(material);
		m_MaterialsProcessed++;

#ifdef DX_DEBUG
		OutputDebugStringA(("MaterialProcessor: Processed material '" + name +
			"' (Model: " + ShadingModels::Name(model) + ")\n").c_str());
#endif

		return material;
	}

	ShadingModel MaterialProcessor::DetermineShadingModel(const aiMaterial* material) const
	{
		if (!material)
		{
			OutputDebugStringA("MaterialProcessor: aiMaterial is null\n");
			return ShadingModel::Unlit;
		}

		// A material the artist marked "no shading" stays unlit.
		int assimpShading = 0;
		if (material->Get(AI_MATKEY_SHADING_MODEL, assimpShading) == AI_SUCCESS && assimpShading == aiShadingMode_NoShading)
			return ShadingModel::Unlit;

		// Everything else is Lit. The old importer also produced "PBR" (when the model had metalness / roughness textures:
		// the same shader), "Emissive" (when the emissive colour was bright: a flat glow that ignored every light) and
		// "Transparent" (opacity < 1: that is a render STATE, see DetermineRenderState). The Lit model reads the emissive
		// colour and map itself, so a glowing part of a model is lit AND glowing.
		return ShadingModel::Lit;
	}

	RenderState MaterialProcessor::DetermineRenderState(const aiMaterial* material) const
	{
		RenderState state;
		float opacity = 1.0f;
		if (material && material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity < 1.0f)
			state = RenderState::Blended();
		return state;
	}

	void MaterialProcessor::LoadBasicProperties(std::shared_ptr<Material> mat, const aiMaterial* aiMat)
	{
		// Load color properties
		aiColor3D diffuse(1.0f, 1.0f, 1.0f);
		aiColor3D specular(1.0f, 1.0f, 1.0f);
		aiColor3D emissive(0.0f, 0.0f, 0.0f);

		aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		aiMat->Get(AI_MATKEY_COLOR_SPECULAR, specular);
		aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);

		// Load scalar properties
		float opacity = 1.0f;
		float shininess = 32.0f;
		float metallic = 0.0f;
		float roughness = 0.5f;

		aiMat->Get(AI_MATKEY_OPACITY, opacity);
		aiMat->Get(AI_MATKEY_SHININESS, shininess);
		aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
		aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);

		// Set material properties
		// The opacity goes into `alpha` ONLY. It used to go into the diffuse colour's alpha as well, and the shaders multiplied
		// both: a 50% transparent model came out 25% opaque.
		mat->SetDiffuseColor({ diffuse.r, diffuse.g, diffuse.b, 1.0f });
		mat->SetSpecularColor({ specular.r, specular.g, specular.b, 1.0f });
		mat->SetEmissiveColor({ emissive.r, emissive.g, emissive.b, 1.0f });
		mat->SetShininess(std::clamp(shininess, 1.0f, 128.0f));
		mat->SetAlpha(opacity);
		mat->SetMetallic(metallic);
		mat->SetRoughness(roughness);
	}

	void MaterialProcessor::LoadAllTextures(
		std::shared_ptr<Material> mat,
		const aiMaterial* aiMat,
		const std::string& directory,
		const ModelLoadOptions& options)
	{
		// Core textures
		LoadTextureOfType(mat, aiMat, directory, aiTextureType_DIFFUSE,  TextureSlot::Diffuse);
		LoadTextureOfType(mat, aiMat, directory, aiTextureType_NORMALS,  TextureSlot::Normal);
		LoadTextureOfType(mat, aiMat, directory, aiTextureType_SPECULAR, TextureSlot::Specular);
		LoadTextureOfType(mat, aiMat, directory, aiTextureType_EMISSIVE, TextureSlot::Emissive);

		// PBR textures
		LoadTextureOfType(mat, aiMat, directory, aiTextureType_METALNESS,         TextureSlot::Metallic);
		LoadTextureOfType(mat, aiMat, directory, aiTextureType_DIFFUSE_ROUGHNESS, TextureSlot::Roughness);
		LoadTextureOfType(mat, aiMat, directory, aiTextureType_AMBIENT_OCCLUSION, TextureSlot::AmbientOcclusion);

		const std::string heightPath =
			LoadTextureOfType(mat, aiMat, directory, aiTextureType_HEIGHT, TextureSlot::Height);
		LoadTextureOfType(mat, aiMat, directory, aiTextureType_OPACITY, TextureSlot::Opacity);

		// Validate height map assignment. Many exporters (OBJ "bump") put a normal map in the HEIGHT slot;
		// the file name tells them apart. (An RHI texture does not remember its path, so the path that
		// LoadTextureOfType returned is used.)
		if (mat->GetTexture(TextureSlot::Height) && !m_TextureLoader->IsHeightMap(heightPath)) {
			// Likely misassigned normal map
			if (!mat->GetTexture(TextureSlot::Normal)) {
				mat->SetTexture(TextureSlot::Normal, mat->GetTexture(TextureSlot::Height));
#ifdef DX_DEBUG
				OutputDebugStringA("MaterialProcessor: Reassigned height texture to normal map\n");
#endif
			}
			mat->SetTexture(TextureSlot::Height, nullptr);   // also switches the HeightMap shader feature off
		}
	}

	std::string MaterialProcessor::LoadTextureOfType(
		std::shared_ptr<Material> mat,
		const aiMaterial* aiMat,
		const std::string& directory,
		aiTextureType type,
		TextureSlot slot)
	{
		std::string relativePath = GetTextureFilename(aiMat, type, directory);

		if (relativePath.empty())
			return "";

		// Resolve the full path
		std::string fullPath = ResolveTexturePath(relativePath, directory);

		if (fullPath.empty()) {
			OutputDebugStringA(("MaterialProcessor: Could not resolve texture path: " +
				relativePath + "\n").c_str());
			return "";
		}

		// colours are read as sRGB, data as linear: the slot table says which (MaterialSlots.cpp)
		auto texture = m_TextureLoader->LoadTexture(fullPath, type, MaterialSlots::Info(slot).colorSpace);

		if (texture && texture->IsValid()) {
			mat->SetTexture(slot, texture);
#ifdef DX_DEBUG
			OutputDebugStringA(("MaterialProcessor: Loaded " +
				m_TextureLoader->GetTextureTypeName(type) +
				" texture: " + fullPath + "\n").c_str());
#endif
			return fullPath;
		}

		OutputDebugStringA(("MaterialProcessor: Failed to load " +
			m_TextureLoader->GetTextureTypeName(type) +
			" texture: " + fullPath + "\n").c_str());
		return "";
	}

	void MaterialProcessor::ConfigureMaterialFromTextures(std::shared_ptr<Material> mat)
	{
		// (A material's shading model is fixed at construction: nothing here, and no setter, changes it.)

		// Parallax mapping: the shader switch (ENABLE_PARALLAX_MAPPING) follows from the height map being bound; only the
		// strength is configured here.
		if (mat->GetTexture(TextureSlot::Height)) {
			mat->SetHeightScale(0.05f);
		}

		// Configure detail textures (the "details" shader switch itself is kept in sync by SetTexture)
		if (mat->GetTexture(TextureSlot::DetailDiffuse) || mat->GetTexture(TextureSlot::DetailNormal)) {
			mat->SetDetailTextureScale({ 8.0f, 8.0f });
		}
	}

	std::string MaterialProcessor::GetTextureFilename(
		const aiMaterial* material,
		aiTextureType type,
		const std::string& directory)
	{
		if (material->GetTextureCount(type) > 0) {
			aiString path;
			if (material->GetTexture(type, 0, &path) == AI_SUCCESS) {
				std::string texturePath = std::string(path.C_Str());

				// Handle embedded textures (start with '*')
				if (!texturePath.empty() && texturePath[0] == '*')
					return texturePath;

				// Normalize slashes to forward slashes
				std::replace(texturePath.begin(), texturePath.end(), '\\', '/');

				// Strip leading ./ or ../
				while (texturePath.rfind("./", 0) == 0)
					texturePath = texturePath.substr(2);
				while (texturePath.rfind("../", 0) == 0)
					texturePath = texturePath.substr(3);

				return texturePath;
			}
		}
		return "";
	}

	std::string MaterialProcessor::ResolveTexturePath(
		const std::string& texturePath,
		const std::string& modelDirectory)
	{
		namespace fs = std::filesystem;

		// If path is embedded, return as-is
		if (!texturePath.empty() && texturePath[0] == '*')
			return texturePath;

		// Try different path resolution strategies
		std::vector<std::string> pathsToTry;

		// 1. Exact path as specified (might be absolute)
		pathsToTry.push_back(texturePath);

		// 2. Relative to model directory
		if (!modelDirectory.empty()) {
			fs::path modelDir(modelDirectory);
			fs::path texPath(texturePath);
			pathsToTry.push_back((modelDir / texPath).string());
		}

		// 3. Just the filename in model directory
		fs::path textureFilename = fs::path(texturePath).filename();
		if (!modelDirectory.empty()) {
			fs::path modelDir(modelDirectory);
			pathsToTry.push_back((modelDir / textureFilename).string());
		}

		// 4. Look in common texture subdirectories
		if (!modelDirectory.empty()) {
			std::vector<std::string> commonTextureDirs = {
				"textures", "Textures", "TEXTURES",
				"maps", "Maps", "MAPS",
				"materials", "Materials"
			};

			for (const auto& subdir : commonTextureDirs) {
				fs::path modelDir(modelDirectory);
				fs::path combined = modelDir / subdir / textureFilename;
				pathsToTry.push_back(combined.string());

				// Also try parent directory
				fs::path parentCombined = modelDir.parent_path() / subdir / textureFilename;
				pathsToTry.push_back(parentCombined.string());
			}
		}

		// Try each path
		for (const auto& path : pathsToTry) {
			try {
				fs::path fsPath(path);
				if (fs::exists(fsPath)) {
#ifdef DX_DEBUG
					OutputDebugStringA(("MaterialProcessor: Resolved texture path: " +
						texturePath + " -> " + fsPath.string() + "\n").c_str());
#endif
					return fsPath.string();
				}
			}
			catch (const std::exception&) {
				// Invalid path, continue trying
				continue;
			}
		}

		// Could not resolve path
		OutputDebugStringA(("MaterialProcessor: Could not find texture file: " +
			texturePath + " (searched " + std::to_string(pathsToTry.size()) +
			" locations)\n").c_str());

#ifdef DX_DEBUG
		OutputDebugStringA("  Searched paths:\n");
		for (size_t i = 0; i < std::min(pathsToTry.size(), size_t(5)); ++i) {
			OutputDebugStringA(("    " + pathsToTry[i] + "\n").c_str());
		}
		if (pathsToTry.size() > 5) {
			OutputDebugStringA(("    ... and " +
				std::to_string(pathsToTry.size() - 5) + " more\n").c_str());
		}
#endif

		return ""; // Not found
	}
}