#pragma once
#include "ShaderProgram.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <utils/material/MaterialTypes.h>
#include "ShaderVariantManager.h"

namespace DXEngine {

	class ShaderManager
	{
	public:
		ShaderManager();
		~ShaderManager();

		void Initialize();
		void Shutdown();
		void Update();

		// Enhanced interface for mesh-specific variants
		std::shared_ptr<ShaderProgram> GetShaderForMesh(
			const VertexLayout& layout,
			const Material* material,
			MaterialType materialType
		);
		//fallback shader
		std::shared_ptr<ShaderProgram> GetFallbackShader(MaterialType materialType);



		// Configuration
		void EnableHotReload(bool enable);
		void EnableDynamicVariants(bool enable) { m_DynamicVariantsEnabled = enable; }
		bool IsDynamicVariantsEnabled() const { return m_DynamicVariantsEnabled; }

		// Cache management
		void ClearShaderCache();
		void PrecompileCommonShaders();

		// Debug and info
		bool IsShaderLoaded(const std::string& name) const;
		std::string GetShaderInfo() const;
		std::string GetDebugInfo() const;

		// Legacy compatibility methods (deprecated but functional)
		[[deprecated("Use GetShaderForMesh with VertexLayout instead")]]
		bool LoadShader(const std::string& name) { return true; } // No-op for compatibility

		[[deprecated("Use ClearShaderCache instead")]]
		bool ReloadShader(const std::string& name);

		[[deprecated("Use ClearShaderCache instead")]]
		void ReloadAllShaders();


	private:
		void InitializeVariantSystem();
		void CreateDefaultMaterialTypeMappings();


	private:
		bool m_Initialized = false;
		bool m_DynamicVariantsEnabled = true;
		std::string m_DefaultShaderName = "Lit";

		// Material type to shader name mapping (for backward compatibility)
		std::unordered_map<MaterialType, std::string> m_MaterialTypeToShader;

		// Default vertex layout for compatibility mode
		VertexLayout m_DefaultLayout;

		// Reference to the variant manager
		ShaderVariantManager* m_VariantManager;
	};

	class ShaderManagerSingleton
	{
	public:
		static ShaderManager& Instance()
		{
			static ShaderManager instance;
			return instance;
		}

	private:
		ShaderManagerSingleton() = default;
	};

}