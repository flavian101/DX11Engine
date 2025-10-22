#pragma once
#include "RHI/GraphicsDevice.h"
#include "utils/Mesh/Utils/VertexAttribute.h"
#include "utils/material/Material.h"
#include "shaders/ShaderProgram.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>

namespace DXEngine::Rendering {

    // ========== SHADER FEATURE FLAGS ==========
    namespace ShaderFeature {
        constexpr uint32_t None = 0;

        // Vertex Attributes
        constexpr uint32_t HasNormals = 1 << 0;
        constexpr uint32_t HasTangents = 1 << 1;
        constexpr uint32_t HasTexCoords = 1 << 2;
        constexpr uint32_t HasSecondTexCoords = 1 << 3;
        constexpr uint32_t HasVertexColors = 1 << 4;
        constexpr uint32_t HasSkinning = 1 << 5;

        // Texture Maps
        constexpr uint32_t DiffuseMap = 1 << 6;
        constexpr uint32_t NormalMap = 1 << 7;
        constexpr uint32_t SpecularMap = 1 << 8;
        constexpr uint32_t EmissiveMap = 1 << 9;
        constexpr uint32_t EnvironmentMap = 1 << 10;
        constexpr uint32_t RoughnessMap = 1 << 11;
        constexpr uint32_t MetallicMap = 1 << 12;
        constexpr uint32_t AOMap = 1 << 13;
        constexpr uint32_t HeightMap = 1 << 14;
        constexpr uint32_t OpacityMap = 1 << 15;
        constexpr uint32_t DetailDiffuseMap = 1 << 16;
        constexpr uint32_t DetailNormalMap = 1 << 17;
        constexpr uint32_t UseDetailsTextures = 1 << 18;

        // Rendering Features
        constexpr uint32_t Fog = 1 << 19;
        constexpr uint32_t ParallaxMapping = 1 << 20;
        constexpr uint32_t Instancing = 1 << 21;
        constexpr uint32_t AlphaTest = 1 << 22;
        constexpr uint32_t Lighting = 1 << 23;
        constexpr uint32_t Shadows = 1 << 24;
    }

    // ========== SHADER VARIANT KEY ==========
    struct ShaderVariantKey {
        std::string baseName;           // "Lit", "PBR", "Unlit", etc.
        uint32_t featureFlags = 0;      // Combination of ShaderFeature flags
        std::string vertexLayoutHash;   // Hash of vertex layout
        VertexLayout actualLayout;      // REQUIRED: Actual vertex layout for compilation

        bool operator==(const ShaderVariantKey& other) const {
            return baseName == other.baseName &&
                featureFlags == other.featureFlags &&
                vertexLayoutHash == other.vertexLayoutHash;
        }

        std::string ToString() const {
            return baseName + "_" + std::to_string(featureFlags) + "_" + vertexLayoutHash;
        }
    };

    struct ShaderVariantKeyHash {
        size_t operator()(const ShaderVariantKey& key) const {
            size_t h1 = std::hash<std::string>{}(key.baseName);
            size_t h2 = std::hash<uint32_t>{}(key.featureFlags);
            size_t h3 = std::hash<std::string>{}(key.vertexLayoutHash);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    // ========== SHADER CACHE CONFIGURATION ==========
    struct ShaderCacheConfig {
        std::string shaderDirectory = "assets/shaders/";
        bool enableHotReload = false;
        bool enableDebugInfo = false;
        bool enableOptimization = true;
        bool precompileCommonVariants = true;
        size_t maxCachedVariants = 256;

        // Fallback shaders
        std::string fallbackVertexShader = "Lit.vs.hlsl";
        std::string fallbackPixelShader = "Lit.ps.hlsl";
    };

    // ========== SHADER CACHE STATISTICS ==========
    struct ShaderCacheStats {
        size_t totalVariants = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t compilationFailures = 0;
        size_t hotReloads = 0;

        void Reset() {
            totalVariants = cacheHits = cacheMisses = 0;
            compilationFailures = hotReloads = 0;
        }

        float GetHitRate() const {
            size_t total = cacheHits + cacheMisses;
            return total > 0 ? (static_cast<float>(cacheHits) / total) * 100.0f : 0.0f;
        }

        std::string ToString() const;
    };

    
    class ShaderCache {
    public:
        explicit ShaderCache(std::shared_ptr<RHI::IGraphicsDevice> device);
        ~ShaderCache();

        // Initialization
        bool Initialize(const ShaderCacheConfig& config = {});
        void Shutdown();
        void Update(); // For hot reload checking

        // Main Interface - Get shader for mesh/material combination
        std::shared_ptr<ShaderProgram> GetShaderForMaterial(
            const VertexLayout& layout,
            const Material* material
        );

        // Direct variant access
        std::shared_ptr<ShaderProgram> GetShaderVariant(const ShaderVariantKey& key);
        std::shared_ptr<ShaderProgram> GetFallbackShader(MaterialType materialType);

        // Base shader loading (without variants)
        bool LoadBaseShader(const std::string& name,
            const std::string& vsPath,
            const std::string& psPath);
        std::shared_ptr<ShaderProgram> GetBaseShader(const std::string& name);

        // Precompilation
        void PrecompileCommonVariants();
        void PrecompileVariantForMaterial(MaterialType type);

        // Cache Management
        void ClearCache();
        void PruneUnusedVariants();

        // Hot Reload
        void EnableHotReload(bool enable);
        void ReloadShader(const std::string& shaderPath);
        void ReloadAllShaders();

        // Feature Analysis
        uint32_t AnalyzeVertexLayout(const VertexLayout& layout);
        uint32_t AnalyzeMaterial(const Material* material);
        uint32_t CombineFeatures(uint32_t layoutFeatures,
            uint32_t materialFeatures,
            MaterialType materialType);

        // Statistics
        const ShaderCacheStats& GetStats() const { return m_Stats; }
        void ResetStats() { m_Stats.Reset(); }
        std::string GetDebugInfo() const;

        // Configuration
        void SetConfig(const ShaderCacheConfig& config) { m_Config = config; }
        const ShaderCacheConfig& GetConfig() const { return m_Config; }

    private:
        // Shader Entry for base shaders
        struct ShaderEntry {
            std::shared_ptr<ShaderProgram> program;
            std::string vsPath;
            std::string psPath;
            uint64_t lastModified = 0;
        };

        // Variant Entry with usage tracking
        struct VariantEntry {
            std::shared_ptr<ShaderProgram> program;
            size_t lastUsedFrame = 0;
            std::string vsPath;
            std::string psPath;
        };

        // Core Compilation
        std::shared_ptr<ShaderProgram> CreateShaderVariant(
            const ShaderVariantKey& key);

        std::shared_ptr<RHI::IShader> CompileShaderFromFile(
            const std::string& path,
            RHI::ShaderStage stage,
            const std::string& defines = ""
        );

        // Shader Path Resolution
        std::pair<std::string, std::string> GetShaderPaths(MaterialType materialType);
        std::string MaterialTypeToShaderName(MaterialType type) const;

        // Define String Generation
        std::string GenerateDefinesString(uint32_t features, const VertexLayout& layout);

        // Hot Reload Support
        void CheckForFileChanges();
        void UpdateFileTimestamp(const std::string& filePath);
        bool HasFileChanged(const std::string& filePath);
        uint64_t GetFileModificationTime(const std::string& path);

        // Utility
        std::string LoadShaderSource(const std::string& path);
        std::string GenerateVertexLayoutHash(const VertexLayout& layout);
        void LogError(const std::string& message);
        void LogWarning(const std::string& message);
        void LogInfo(const std::string& message);

    private:
        std::shared_ptr<RHI::IGraphicsDevice> m_Device;
        ShaderCacheConfig m_Config;
        ShaderCacheStats m_Stats;

        // Cache Storage
        std::unordered_map<std::string, ShaderEntry> m_BaseShaders;
        std::unordered_map<ShaderVariantKey, VariantEntry, ShaderVariantKeyHash> m_VariantCache;
        std::unordered_map<MaterialType, std::shared_ptr<ShaderProgram>> m_FallbackShaders;

        // Hot Reload Tracking
        std::unordered_map<std::string, uint64_t> m_FileTimestamps;

        // Usage Tracking
        size_t m_CurrentFrame = 0;

        // Thread Safety
        mutable std::mutex m_CacheMutex;

        bool m_Initialized = false;
    };

    // UTILITY FUNCTIONS 
    namespace ShaderUtils {
        // Feature flag helpers
        bool HasFeature(uint32_t flags, uint32_t feature);
        void SetFeature(uint32_t& flags, uint32_t feature, bool enabled);
        std::string FeaturesToString(uint32_t flags);

        // Material helpers
        MaterialType DeduceMaterialType(const Material* material);
        uint32_t ExtractMaterialFeatures(const Material* material);
    }
}