#include "dxpch.h"
#include "ShaderManager.h"
#include <filesystem>
#include <codecvt>

namespace DXEngine {

	ShaderManager::ShaderManager()
		:m_DefaultShaderName("DefaultLit")
	{}

	ShaderManager::~ShaderManager()
	{
		Shutdown();
	}

	void ShaderManager::Initialize()
	{
		CreateDefaultShaderConfigs();
		LoadDefaultShaders();
	}

	void ShaderManager::Shutdown()
	{
		m_Shaders.clear();
		m_MaterialTypeToShader.clear();
		m_ShaderConfigs.clear();
		m_ShaderFileTimestamps.clear();
	}
    void ShaderManager::CreateDefaultShaderConfigs()
    {
        // Define all shader configurations
        std::vector<ShaderConfig> configs = {
            // Basic unlit shader
            ShaderConfig(
                "assets/shaders/Unlit.vs.cso",
                "assets/shaders/Unlit.ps.cso",
                "Unlit",
                MaterialType::Unlit
            ),

                // Basic lit shader
                ShaderConfig(
                    "assets/shaders/Lit.vs.cso",
                    "assets/shaders/Lit.ps.cso",
                    "Lit",
                    MaterialType::Lit
                ),

                // Textured lit shader
                ShaderConfig(
                    "assets/shaders/LitTextured.vs.cso",
                    "assets/shaders/LitTextured.ps.cso",
                    "LitTextured",
                    MaterialType::LitTextured
                ),

                // Normal mapped shader
                ShaderConfig(
                    "assets/shaders/NormalMapped.vs.cso",
                    "assets/shaders/NormalMapped.ps.cso",
                    "NormalMapped",
                    MaterialType::LitNormalMapped
                ),

                // Skybox shader
                ShaderConfig(
                    "assets/shaders/Skybox.vs.cso",
                    "assets/shaders/Skybox.ps.cso",
                    "Skybox",
                    MaterialType::Skybox
                ),

                // Transparent shader
                ShaderConfig(
                    "assets/shaders/Transparent.vs.cso",
                    "assets/shaders/Transparent.ps.cso",
                    "Transparent",
                    MaterialType::Transparent
                ),

                // Emissive shader
                ShaderConfig(
                    "assets/shaders/Emissive.vs.cso",
                    "assets/shaders/Emissive.ps.cso",
                    "Emissive",
                    MaterialType::Emissive
                ),

                // UI shader
                ShaderConfig(
                    "assets/shaders/UI.vs.cso",
                    "assets/shaders/UI.ps.cso",
                    "UI",
                    MaterialType::UI
                )
        };

        // Store configs
        for (const auto& config : configs)
        {
            m_ShaderConfigs[config.name] = config;
        }

        // Map material types to shaders
        m_MaterialTypeToShader[MaterialType::Unlit] = "Unlit";
        m_MaterialTypeToShader[MaterialType::Lit] = "Lit";
        m_MaterialTypeToShader[MaterialType::LitTextured] = "LitTextured";
        m_MaterialTypeToShader[MaterialType::LitNormalMapped] = "NormalMapped";
        m_MaterialTypeToShader[MaterialType::Skybox] = "Skybox";
        m_MaterialTypeToShader[MaterialType::Transparent] = "Transparent";
        m_MaterialTypeToShader[MaterialType::Emissive] = "Emissive";
        m_MaterialTypeToShader[MaterialType::UI] = "UI";
        m_MaterialTypeToShader[MaterialType::PBR] = "Lit"; // Fallback for now
    }

    void ShaderManager::LoadDefaultShaders()
    {
        // Load all configured shaders
        for (const auto& [name, config] : m_ShaderConfigs)
        {
            if (!LoadShader(config))
            {
                // Log warning but continue
                OutputDebugStringA(("Failed to load shader: " + name + "\n").c_str());
            }
        }

        // Ensure we have at least one working shader
        if (m_Shaders.empty())
        {
            // Create emergency fallback shader
            OutputDebugStringA("Warning: No shaders loaded successfully!\n");
        }
    }
    
    std::shared_ptr<ShaderProgram> ShaderManager::GetShader(MaterialType materialType)
    {
        auto it = m_MaterialTypeToShader.find(materialType);
        if (it != m_MaterialTypeToShader.end())
        {
            return GetShader(it->second);
        }

        // Fallback to default shader
        return GetShader(m_DefaultShaderName);
    }

    std::shared_ptr<ShaderProgram> ShaderManager::GetShader(const std::string& name)
    {
        auto it = m_Shaders.find(name);
        if (it != m_Shaders.end())
        {
            return it->second;
        }

        //try to load the shader if config exists
        auto configIt = m_ShaderConfigs.find(name);
        if (configIt != m_ShaderConfigs.end())
        {
            if (LoadShader(configIt->second))
            {
                return m_Shaders[name];
            }
        }

        //default fall back
        auto defaultIt = m_Shaders.find(m_DefaultShaderName);
        if (defaultIt != m_Shaders.end())
        {
            return defaultIt->second;
        }

        //return last resort 
        if (!m_Shaders.empty())
        {
            return m_Shaders.begin()->second;
        }

        return nullptr;
    }

    bool ShaderManager::LoadShader(const ShaderConfig& config)
    {
        try
        {
            std::wstring vsPath = StringToWString(config.vertexShaderPath);
            std::wstring psPath = StringToWString(config.pixelShaderPath);

            // Check if files exist
            if (!std::filesystem::exists(config.vertexShaderPath) ||
                !std::filesystem::exists(config.pixelShaderPath))
            {
                OutputDebugStringA(("Shader files not found for: " + config.name + "\n").c_str());
                return false;
            }

            auto shader = std::make_shared<ShaderProgram>(vsPath.c_str(), psPath.c_str());

            if (shader)
            {
                m_Shaders[config.name] = shader;

                // Store file timestamps for hot reload
                if (m_HotReloadEnabled)
                {
                    try
                    {
                        auto vsTime = std::filesystem::last_write_time(config.vertexShaderPath);
                        auto psTime = std::filesystem::last_write_time(config.pixelShaderPath);
                        m_ShaderFileTimestamps[config.vertexShaderPath] =
                            std::chrono::duration_cast<std::chrono::milliseconds>(vsTime.time_since_epoch()).count();
                        m_ShaderFileTimestamps[config.pixelShaderPath] =
                            std::chrono::duration_cast<std::chrono::milliseconds>(psTime.time_since_epoch()).count();
                    }
                    catch (...)
                    {
                        // Ignore timestamp errors
                    }
                }
                OutputDebugStringA(("Successfully loaded shader: " + config.name + "\n").c_str());
                return true;
            }
        }
        catch (const std::exception& e)
        {
            OutputDebugStringA(("Exception loading shader " + config.name + ": " + e.what() + "\n").c_str());
        }
        catch (...)
        {
            OutputDebugStringA(("Unknown exception loading shader: " + config.name + "\n").c_str());
        }

        return false;
    }

    bool ShaderManager::ReloadShader(const std::string& name)
    {
        auto configIt = m_ShaderConfigs.find(name);
        if (configIt == m_ShaderConfigs.end())
        {
            return false;
        }

        // Remove old shader
        m_Shaders.erase(name);

        // Load new shader
        return LoadShader(configIt->second);
    }

    void ShaderManager::ReloadAllShaders()
    {
        OutputDebugStringA("Reloading all shaders...\n");

        for (const auto& [name, config] : m_ShaderConfigs)
        {
            ReloadShader(name);
        }
    }

    void ShaderManager::RegisterShaderForMaterialType(MaterialType type, const std::string& shaderName)
    {
        m_MaterialTypeToShader[type] = shaderName;
    }

    bool ShaderManager::IsShaderLoaded(const std::string& name) const
    {
        return m_Shaders.find(name) != m_Shaders.end();
    }

    std::string ShaderManager::GetShaderInfo() const
    {
        std::string info = "Loaded Shaders:\n";
        for (const auto& [name, shader] : m_Shaders)
        {
            info += "  - " + name + "\n";
        }

        info += "\nMaterial Type Mappings:\n";
        for (const auto& [type, shaderName] : m_MaterialTypeToShader)
        {
            info += "  - Type " + std::to_string(static_cast<int>(type)) + " -> " + shaderName + "\n";
        }

        return info;
    }

    void ShaderManager::CheckForShaderChanges()
    {
        if (!m_HotReloadEnabled)
            return;

        bool needsReload = false;

        for (const auto& [filePath, storedTime] : m_ShaderFileTimestamps)
        {
            try
            {
                if (std::filesystem::exists(filePath))
                {
                    auto currentTime = std::filesystem::last_write_time(filePath);
                    auto currentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        currentTime.time_since_epoch()).count();

                    if (currentTimeMs != storedTime)
                    {
                        needsReload = true;
                        break;
                    }
                }
            }
            catch (...)
            {
                // Ignore file system errors
            }
        }

        if (needsReload)
        {
            ReloadAllShaders();
        }
    }

    std::wstring ShaderManager::StringToWString(const std::string& str)
    {
        if (str.empty())
            return std::wstring();

        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(str);
    }
}