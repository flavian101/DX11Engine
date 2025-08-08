#pragma once
#include "ShaderProgram.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <utils/material/MaterialTypes.h>

namespace DXEngine {
	struct ShaderConfig
	{
		std::string vertexShaderPath;
		std::string pixelShaderPath;
		std::string name;
		MaterialType materialType;

		ShaderConfig() = default;
		ShaderConfig(const std::string& vsPath, const std::string& psPath,const std::string& shaderName, MaterialType type = MaterialType::Lit)
			:vertexShaderPath(vsPath), pixelShaderPath(psPath), name(shaderName), materialType(type)
		{}
	};

	class ShaderManager
	{
	public:
		ShaderManager();
		~ShaderManager();

		void Initialize();
		void Shutdown();

		//shader managment
		std::shared_ptr<ShaderProgram> GetShader(MaterialType materialType);
		std::shared_ptr<ShaderProgram> GetShader(const std::string& name);

		bool LoadShader(const ShaderConfig& config);
		bool ReloadShader(const std::string& name);
		void ReloadAllShaders();

		//shader registration for diffrent material types
		void RegisterShaderForMaterialType(MaterialType type, const std::string& shaderName);


		bool IsShaderLoaded(const std::string& name) const;
		std::string GetShaderInfo()const;

		void EnableHotReload(bool enable) { m_HotReloadEnabled = enable; };
		void CheckForShaderChanges();

	private:
		void LoadDefaultShaders();
		void CreateDefaultShaderConfigs();
		std::wstring StringToWString(const std::string& str);

	private:
		std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> m_Shaders;
		std::unordered_map<MaterialType, std::string> m_MaterialTypeToShader;
		std::unordered_map<std::string, ShaderConfig> m_ShaderConfigs;
		std::string m_DefaultShaderName;

		//hotreaload
		bool m_HotReloadEnabled = false;
		std::unordered_map<std::string, uint64_t> m_ShaderFileTimestamps;
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