#include "dxpch.h"
#include "ShaderManager.h"
#include <filesystem>
#include <codecvt>

namespace DXEngine {


    ShaderManager::ShaderManager()
        : m_VariantManager(&ShaderVariantManager::Instance()) {
        CreateDefaultMaterialTypeMappings();
        m_DefaultLayout = VertexLayout::CreateLit(); // Standard layout with tangents
    }

    ShaderManager::~ShaderManager() {
        Shutdown();
    }

    void ShaderManager::Initialize() {
        if (m_Initialized) {
            return;
        }

        InitializeVariantSystem();
        m_Initialized = true;

        OutputDebugStringA("ShaderManager: Initialized with variant system\n");
    }

    void ShaderManager::Shutdown() {
        if (!m_Initialized) {
            return;
        }

        // ShaderVariantManager is a singleton, so we don't shut it down here
        // Just clean up our local state
        m_MaterialTypeToShader.clear();
        m_Initialized = false;

        OutputDebugStringA("ShaderManager: Shutdown complete\n");
    }

    void ShaderManager::Update() {
        if (m_Initialized && m_VariantManager) {
            m_VariantManager->Update();
        }
    }

    std::shared_ptr<ShaderProgram> ShaderManager::GetShader(MaterialType materialType) {
        if (!m_Initialized || !m_VariantManager) {
            return nullptr;
        }

        if (m_DynamicVariantsEnabled) {
            // Use variant system with default layout
            return m_VariantManager->GetShaderVariant(m_DefaultLayout, nullptr, materialType);
        }
        else {
            // Legacy compatibility - create variant key manually
            ShaderVariantKey key;
            key.materialType = materialType;
            key.vertexLayoutHash = m_VariantManager->GenerateVertexLayoutHash(m_DefaultLayout);
            key.features = m_VariantManager->CombineFeatures(
                m_VariantManager->AnalyzeVertexLayout(m_DefaultLayout),
                ShaderFeatureFlags{},
                materialType
            );
            return m_VariantManager->GetShaderVariant(key);
        }
    }

    std::shared_ptr<ShaderProgram> ShaderManager::GetShader(const std::string& name) {
        if (!m_Initialized || !m_VariantManager) {
            return nullptr;
        }

        // Map shader name to material type (backward compatibility)
        MaterialType materialType = MaterialType::Lit; // Default

        if (name == "Unlit") materialType = MaterialType::Unlit;
        else if (name == "Lit") materialType = MaterialType::Lit;
        else if (name == "Transparent") materialType = MaterialType::Transparent;
        else if (name == "Emissive") materialType = MaterialType::Emissive;
        else if (name == "Skybox") materialType = MaterialType::Skybox;
        else if (name == "UI") materialType = MaterialType::UI;

        return GetShader(materialType);
    }

    std::shared_ptr<ShaderProgram> ShaderManager::GetShaderForMesh(
        const VertexLayout& layout,
        const Material* material,
        MaterialType materialType) {

        if (!m_Initialized || !m_VariantManager) {
            return GetFallbackShader();
        }

        if (m_DynamicVariantsEnabled) {
            auto shader = m_VariantManager->GetShaderVariant(layout, material, materialType);
            if (shader) {
                return shader;
            }

            // Fallback to standard shader if variant fails
            OutputDebugStringA("Warning: Failed to create shader variant, falling back to standard shader\n");
        }

        // Fallback to basic material type shader
        return GetShader(materialType);
    }

    void ShaderManager::EnableHotReload(bool enable) {
        if (m_VariantManager) {
            m_VariantManager->EnableHotReload(enable);
        }
    }

    void ShaderManager::ClearShaderCache() {
        if (m_VariantManager) {
            m_VariantManager->ClearCache();
        }
    }

    void ShaderManager::PrecompileCommonShaders() {
        if (m_VariantManager) {
            m_VariantManager->PrecompileCommonVariants();
        }
    }

    bool ShaderManager::IsShaderLoaded(const std::string& name) const {
        // For compatibility, always return true since variants are created on-demand
        return true;
    }

    std::string ShaderManager::GetShaderInfo() const {
        if (!m_VariantManager) {
            return "ShaderManager: Not initialized\n";
        }

        std::string info = "=== Shader Manager Info ===\n";
        info += "Dynamic Variants: " + std::string(m_DynamicVariantsEnabled ? "Enabled" : "Disabled") + "\n";
        info += "Default Shader: " + m_DefaultShaderName + "\n";
        info += "\nMaterial Type Mappings:\n";

        for (const auto& [type, shaderName] : m_MaterialTypeToShader) {
            info += "  - Type " + std::to_string(static_cast<int>(type)) + " -> " + shaderName + "\n";
        }

        info += "\n" + m_VariantManager->GetDebugInfo();
        return info;
    }

    std::string ShaderManager::GetDebugInfo() const {
        return GetShaderInfo();
    }

    bool ShaderManager::ReloadShader(const std::string& name) {
        // Legacy compatibility - clear cache to force reload
        ClearShaderCache();
        return true;
    }

    void ShaderManager::ReloadAllShaders() {
        if (m_VariantManager) {
            m_VariantManager->ReloadAllShaders();
        }
    }

    void ShaderManager::InitializeVariantSystem() {
        // Initialize variant manager with appropriate config
        ShaderVariantConfig config;
        config.shaderBasePath = "assets/shaders/";
        config.enableHotReload = false; // Start disabled, enable explicitly
        config.enableDebugInfo = true;
        config.precompileCommonVariants = true;

        if (!m_VariantManager->Initialize(config)) {
            OutputDebugStringA("ShaderManager: Failed to initialize variant system\n");
        }
    }

    void ShaderManager::CreateDefaultMaterialTypeMappings() {
        m_MaterialTypeToShader[MaterialType::Unlit] = "Unlit";
        m_MaterialTypeToShader[MaterialType::Lit] = "Lit";
        m_MaterialTypeToShader[MaterialType::Skybox] = "Skybox";
        m_MaterialTypeToShader[MaterialType::Transparent] = "Transparent";
        m_MaterialTypeToShader[MaterialType::Emissive] = "Emissive";
        m_MaterialTypeToShader[MaterialType::UI] = "UI";
    }

    std::shared_ptr<ShaderProgram> ShaderManager::GetFallbackShader() {
        // Try to get a basic lit shader as fallback
        if (m_VariantManager) {
            return GetShader(MaterialType::Lit);
        }
        return nullptr;
    }
}