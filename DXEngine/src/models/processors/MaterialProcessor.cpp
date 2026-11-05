#include "dxpch.h"
#include "MaterialProcessor.h"
#include "utils/material/Material.h"
#include "TextureLoader.h"
#include "RHI/GraphicsDevice.h"
#include "renderer/ShaderCache.h"   // ShaderFeature flags
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

		MaterialType type = DetermineMaterialType(aiMaterial);
		auto material = std::make_shared<Material>(name, type);

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
			"' (Type: " + std::to_string(static_cast<int>(type)) + ")\n").c_str());
#endif

		return material;
	}

	MaterialType MaterialProcessor::DetermineMaterialType(const aiMaterial* material) const
	{
		if (!material)
		{
			OutputDebugStringA("MaterialProcessor: aiMaterial is null\n");
			return MaterialType::Unlit;
		}

		// Check for transparency
		float opacity = 1.0f;
		if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity < 1.0f)
		{
			return MaterialType::Transparent;
		}

		// Check for emissive properties
		aiColor3D emissive(0.0f, 0.0f, 0.0f);
		if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
			if (emissive.r > 0.1f || emissive.g > 0.1f || emissive.b > 0.1f) {
				return MaterialType::Emissive;
			}
		}

		// Check for PBR workflow
		if (material->GetTextureCount(aiTextureType_METALNESS) > 0 ||
			material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
			return MaterialType::PBR;
		}

		return MaterialType::Lit;
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
		mat->SetDiffuseColor({ diffuse.r, diffuse.g, diffuse.b, opacity });
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

		auto texture = m_TextureLoader->LoadTexture(fullPath, type);

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
		// (The old Lit -> PBR promotion is gone: a material's type is fixed at construction now and
		// DetermineMaterialType() already picks PBR when the model has metalness/roughness textures.)

		// Configure parallax mapping
		if (mat->GetTexture(TextureSlot::Height)) {
			mat->EnableFeature(ShaderFeature::ParallaxMapping);
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