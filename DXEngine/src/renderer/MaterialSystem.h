#pragma once
#include "RHI/GraphicsDevice.h"
#include "utils/material/Material.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace DXEngine::Rendering
{
	class ShaderCache;
	class ShaderProgram;
	struct ShaderVariantKey;

	class MaterialSystem
	{
	public:
		MaterialSystem(std::shared_ptr<RHI::IGraphicsDevice> device, std::shared_ptr<ShaderCache> shaderCache);
		~MaterialSystem();

		bool Initialize();
		void Shutdown();

		std::shared_ptr<Material> CreateMaterial(const std::string& name,
			MaterialType type);
		std::shared_ptr<Material> GetMaterial(const std::string& name);
		std::shared_ptr<Material> GetDefaultMaterial(MaterialType type);

		// ========== Material Management ==========
		void UpdateMaterials();  // Update dirty material constant buffers
		void RemoveMaterial(const std::string& name);

		// ========== Shader Resolution ==========
		std::shared_ptr<ShaderProgram> GetShaderForMaterial(Material* material);

		// ========== Statistics ==========
		size_t GetMaterialCount() const;


	private:
		void CreateDefaultMaterials();
		ShaderVariantKey CreateVariantKey(Material* material);


	private:
		std::shared_ptr<RHI::IGraphicsDevice> m_Device;
		std::shared_ptr<ShaderCache> m_ShaderCache;

		std::unordered_map<std::string, std::shared_ptr<Material>> m_Materials;
		std::unordered_map<MaterialType, std::shared_ptr<Material>> m_DefaultMaterials;


	};
}

