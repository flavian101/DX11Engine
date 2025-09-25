#pragma once
#include "ShaderProgram.h"
#include "utils/Mesh/Utils/VertexAttribute.h"
#include "utils/material/MaterialTypes.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <bitset>
#include <vector>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <mutex>

namespace DXEngine
{
	class Material;

    enum class ShaderFeature : uint32_t {
        // Texture features
        HasDiffuseTexture = 0,
        HasNormalMap = 1,
        HasSpecularMap = 2,
        HasEmissiveMap = 3,
        HasEnvironmentMap = 4,

        // Vertex attribute features
        HasTangent = 8,
        HasVertexColor = 9,
        HasBlendWeights = 10,
        HasBlendIndices = 11,
        HasSecondUV = 12,

        // Rendering features
        EnableShadows = 16,
        EnableFog = 17,
        EnableInstancing = 18,
        EnableAlphaTest = 19,
        EnableEmissive = 20,

        // Advanced features
        EnableParallaxMapping = 24,
        EnableSubsurfaceScattering = 25,
        EnableDetailMapping = 26,

        MaxFeatures = 32
    };

    using ShaderFeatureFlags = std::bitset<32>;

    struct ShaderVariantKey {
        MaterialType materialType;
        ShaderFeatureFlags features;
        std::string vertexLayoutHash;

        bool operator==(const ShaderVariantKey& other) const {
            return materialType == other.materialType &&
                features == other.features &&
                vertexLayoutHash == other.vertexLayoutHash;
        }

        std::string ToString() const {
            return std::to_string(static_cast<int>(materialType)) + "_" +
                std::to_string(features.to_ulong()) + "_" + vertexLayoutHash;
        }
    };

    struct ShaderVariantKeyHash {
        size_t operator()(const ShaderVariantKey& key) const {
            size_t h1 = std::hash<int>{}(static_cast<int>(key.materialType));
            size_t h2 = std::hash<unsigned long>{}(key.features.to_ulong());
            size_t h3 = std::hash<std::string>{}(key.vertexLayoutHash);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    //cofiguration for shaderPaths and compilation
    struct ShaderVariantConfig
    {
        std::string shaderBasePath = "assets/shaders/";
        bool enableHotReload = false;
        bool enableDebugInfo = false;
        bool enableOptimization = true;
        bool precompileCommonVariants = true;

        //fall back options
        std::string fallbackVertexShader = "Lit.vs.hlsl";
        std::string fallbackPixelShader = "Lit.ps.hlsl";

    };

    //stastics and Debug info
    struct ShaderVariantStats
    {
        size_t totalVariants = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t compilationFailures = 0;
        size_t hotReloads = 0;

        void Reset()
        {
            totalVariants = cacheHits = cacheMisses = compilationFailures = hotReloads = 0;

        }

        std::string ToString()const
        {
            float hitRate = (cacheHits + cacheMisses) > 0 ?
                ((float)(cacheHits) / float(cacheHits + cacheMisses)) * 100.0f : 0.0f;

            return "Shader Stats:\n" +
                std::string("  Total Variants: ") + std::to_string(totalVariants) + "\n" +
                std::string("  Cache Hit Rate: ") + std::to_string(hitRate) + "%\n" +
                std::string("  Compilation Failures: ") + std::to_string(compilationFailures) + "\n" +
                std::string("  Hot Reloads: ") + std::to_string(hotReloads) + "\n";
        }
    };


	class ShaderVariantManager
	{
    public:
        static ShaderVariantManager& Instance()
        {
            static ShaderVariantManager instance;
            return instance;
        }

        // Core functionality
        bool Initialize(const ShaderVariantConfig& config = {});
        void Shutdown();
        void Update(); // For hot reload checking

        // Main interface - gets shader for specific mesh/material combination
        std::shared_ptr<ShaderProgram> GetShaderVariant(
            const VertexLayout& layout,
            const Material* material,
            MaterialType materialType
        );

        // Direct variant access by key
        std::shared_ptr<ShaderProgram> GetShaderVariant(const ShaderVariantKey& key);

        // Precompilation
        void PrecompileCommonVariants();
        void PrecompileVariant(const ShaderVariantKey& key);

        // Cache management
        void ClearCache();
        void PruneLeastUsedVariants(size_t maxVariants = 256);

        // Hot reload
        void EnableHotReload(bool enable);
        void ReloadShader(const std::string& shaderPath);
        void ReloadAllShaders();

        // Configuration
        void SetConfig(const ShaderVariantConfig& config) { m_Config = config; }
        const ShaderVariantConfig& GetConfig() const { return m_Config; }

        // Feature analysis
        ShaderFeatureFlags AnalyzeVertexLayout(const VertexLayout& layout);
        ShaderFeatureFlags AnalyzeMaterial(const Material* material);
        ShaderFeatureFlags CombineFeatures(
            const ShaderFeatureFlags& layoutFeatures,
            const ShaderFeatureFlags& materialFeatures,
            MaterialType materialType
        );

        std::string GenerateVertexLayoutHash(const VertexLayout& layout);


        // Statistics and debugging
        const ShaderVariantStats& GetStats() const { return m_Stats; }
        void ResetStats() { m_Stats.Reset(); }
        std::vector<std::string> GetLoadedVariantNames() const;
        std::string GetDebugInfo() const;

    private:
        ShaderVariantManager() = default;
        ~ShaderVariantManager() = default;
        ShaderVariantManager(const ShaderVariantManager&) = delete;
        ShaderVariantManager& operator=(const ShaderVariantManager&) = delete;

        // Core compilation pipeline
        std::shared_ptr<ShaderProgram> CreateShaderVariant(const ShaderVariantKey& key, const VertexLayout& layout);
        Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
            const std::string& filePath,
            const std::string& defines,
            const std::string& target,
            const std::string& entryPoint = "main"
        );

       

        // Shader path resolution
        std::pair<std::string, std::string> GetShaderPaths(MaterialType materialType);
        std::string GenerateDefinesString(const ShaderFeatureFlags& features, const VertexLayout& layout);

        // Hot reload support
        void CheckForFileChanges();
        void UpdateFileTimestamp(const std::string& filePath);
        bool HasFileChanged(const std::string& filePath);

        // Utility functions
        std::string MaterialTypeToString(MaterialType type)const;
        std::wstring StringToWString(const std::string& str);
        void LogError(const std::string& message);
        void LogWarning(const std::string& message);
        void LogInfo(const std::string& message);

    private:
        ShaderVariantConfig m_Config;
        ShaderVariantStats m_Stats;

        // Main variant cache
        std::unordered_map<ShaderVariantKey, std::shared_ptr<ShaderProgram>, ShaderVariantKeyHash> m_VariantCache;

        // Usage tracking for cache pruning
        std::unordered_map<ShaderVariantKey, size_t, ShaderVariantKeyHash> m_VariantUsage;
        size_t m_CurrentFrame = 0;

        // Hot reload support
        std::unordered_map<std::string, uint64_t> m_FileTimestamps;
        std::vector<std::string> m_TrackedFiles;

        // Common vertex layouts for precompilation
        std::vector<VertexLayout> m_CommonLayouts;

        // Thread safety
        mutable std::mutex m_CacheMutex;

        bool m_Initialized = false;

	};

    // Utility functions for external use
    namespace ShaderVariantUtils {
        // Feature flag helpers
        ShaderFeatureFlags CreateFeatureFlags(const std::vector<ShaderFeature>& features);
        bool HasFeature(const ShaderFeatureFlags& flags, ShaderFeature feature);
        void SetFeature(ShaderFeatureFlags& flags, ShaderFeature feature, bool enabled);
        std::string FeaturesToString(const ShaderFeatureFlags& flags);

        // Vertex layout analysis
        bool IsStandardLayout(const VertexLayout& layout);
        std::string GetLayoutDescription(const VertexLayout& layout);

        // Material analysis
        ShaderFeatureFlags ExtractMaterialFeatures(const Material* material);
        MaterialType DeduceMaterialType(const Material* material);

        // HLSL generation helpers
        std::string GenerateVertexInputStructure(const VertexLayout& layout);
        std::string GetHLSLDataType(DataFormat format);
        std::string GetSemanticName(VertexAttributeType type);
    }
}

