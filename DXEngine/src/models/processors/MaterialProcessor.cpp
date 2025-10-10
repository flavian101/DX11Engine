#include "dxpch.h"
#include "MaterialProcessor.h"
#include "utils/material/Material.h"
#include "TextureLoader.h"
#include "utils/Texture.h"


namespace DXEngine
{
	MaterialProcessor::MaterialProcessor(std::shared_ptr<TextureLoader> textureLoader)
		:
		m_TextureLoader(textureLoader),
		m_MaterialsProcessed(0)
	{
		if (!m_TextureLoader)
		{
			m_TextureLoader = std::make_shared<TextureLoader>();
		}
	}
	std::shared_ptr<Material> MaterialProcessor::ProcessMaterial(const aiMaterial* aiMaterial, const std::string& directory, const ModelLoadOptions& options)
	{
		if (!aiMaterial)
		{
			OutputDebugStringA("Material Processor: Null aiMaterial");
			return nullptr;
		}

		//Get Material name
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

		//configure Material based on loaded textures
		ConfigureMaterialFromTextures(material);
		m_MaterialsProcessed++;

		return material;
	}
	MaterialType MaterialProcessor::DetermineMaterialType(const aiMaterial* material) const
	{
		if (!material)
		{
			OutputDebugStringA("Material Processor: aiMaterial in null");
			return MaterialType::Unlit;
		}

		//check for transparency
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
	void MaterialProcessor::LoadAllTextures(std::shared_ptr<Material> mat, const aiMaterial* aiMat, const std::string& directory, const ModelLoadOptions& options)
	{
		// Core textures
		LoadTextureOfType(mat, aiMat, directory, aiTextureType_DIFFUSE,
			[mat](auto tex) { mat->SetDiffuseTexture(tex); });

		LoadTextureOfType(mat, aiMat, directory, aiTextureType_NORMALS,
			[mat](auto tex) { mat->SetNormalTexture(tex); });

		LoadTextureOfType(mat, aiMat, directory, aiTextureType_SPECULAR,
			[mat](auto tex) { mat->SetSpecularTexture(tex); });

		LoadTextureOfType(mat, aiMat, directory, aiTextureType_EMISSIVE,
			[mat](auto tex) { mat->SetEmissiveTexture(tex); });

		// PBR textures
		LoadTextureOfType(mat, aiMat, directory, aiTextureType_METALNESS,
			[mat](auto tex) { mat->SetMetallicTexture(tex); });

		LoadTextureOfType(mat, aiMat, directory, aiTextureType_DIFFUSE_ROUGHNESS,
			[mat](auto tex) { mat->SetRoughnessTexture(tex); });

		LoadTextureOfType(mat, aiMat, directory, aiTextureType_AMBIENT_OCCLUSION,
			[mat](auto tex) { mat->SetAOTexture(tex); });

		LoadTextureOfType(mat, aiMat, directory, aiTextureType_HEIGHT,
			[mat](auto tex) { mat->SetHeightTexture(tex); });

		LoadTextureOfType(mat, aiMat, directory, aiTextureType_OPACITY,
			[mat](auto tex) { mat->SetOpacityTexture(tex); });

		// Validate height map assignment
		if (mat->HasHeightTexture() && !m_TextureLoader->IsHeightMap(mat->GetHeightTexture())) {
			// Likely misassigned normal map
			if (!mat->HasNormalTexture()) {
				mat->SetNormalTexture(mat->GetHeightTexture());
#ifdef DX_DEBUG
				OutputDebugStringA("MaterialProcessor: Reassigned height texture to normal map\n");
#endif
			}
			mat->SetHeightTexture(nullptr);
		}
	}
	void MaterialProcessor::LoadTextureOfType(std::shared_ptr<Material> mat, const aiMaterial* aiMat, const std::string& directory, aiTextureType type, std::function<void(std::shared_ptr<Texture>)> setter)
	{
		std::string texturePath = GetTextureFilename(aiMat, type, directory);

		if (!texturePath.empty()) {
			auto texture = m_TextureLoader->LoadTexture(texturePath, type);

			if (texture && texture->IsValid()) {
				setter(texture);
#ifdef DX_DEBUG
				OutputDebugStringA(("MaterialProcessor: Loaded " +
					m_TextureLoader->GetTextureTypeName(type) +
					" texture: " + texturePath + "\n").c_str());
#endif
			}
			else {
				OutputDebugStringA(("MaterialProcessor: Failed to load " +
					m_TextureLoader->GetTextureTypeName(type) +
					" texture: " + texturePath + "\n").c_str());
			}
		}
	}
	void MaterialProcessor::ConfigureMaterialFromTextures(std::shared_ptr<Material> mat)
	{
		// Update material type based on loaded textures
		if (mat->HasRoughnessTexture() || mat->HasMetallicTexture()) {
			if (mat->GetType() == MaterialType::Lit) {
				mat->SetType(MaterialType::PBR);
			}
		}

		// Configure parallax mapping
		if (mat->HasHeightTexture()) {
			mat->SetFlag(MaterialFlags::UseParallaxMapping, true);
			mat->SetHeightScale(0.05f);
		}

		// Configure detail textures
		if (mat->IsTextureSlotUsed(TextureSlot::DetailDiffuse) ||
			mat->IsTextureSlotUsed(TextureSlot::DetailNormal)) {
			mat->SetFlag(MaterialFlags::UseDetailTextures, true);
			mat->SetDetailTextureScale({ 8.0f, 8.0f });
		}
	}
	std::string MaterialProcessor::GetTextureFilename(const aiMaterial* material, aiTextureType type, const std::string& directory)
	{
		if (material->GetTextureCount(type) > 0) {
			aiString path;
			if (material->GetTexture(type, 0, &path) == AI_SUCCESS) {
				std::string texturePath = std::string(path.C_Str());

				// Handle embedded textures
				if (!texturePath.empty() && texturePath[0] == '*')
					return texturePath;

				// Normalize slashes
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
}