#pragma once
#include "utils/Mesh/Utils/VertexAttribute.h"
#include "utils/material/MaterialTypes.h"
#include <string>
#include <unordered_map>
#include <bitset>

namespace DXEngine
{
    class ShaderProgram;
    class Material;
    
    enum class ShaderFeature : uint32_t
    {
        HasDiffuseTexture = 0,
        HasNormalMap = 1,
        HasSpecularMap = 2,
        HasEmissiveMap = 3,
        HasTangent = 4,
        HasVertexColor = 5,
        HasBlendWeights = 6,
        HasBlendIndices = 7,
        HasSecondUV = 8,
        IsInstanced = 9,
        EnableShadows = 10,
        EnableFog = 11,
        MaxFeatures = 32  // Retained as-is for bitset size; not used as a feature index
    };
    using ShaderFeatureFlags = std::bitset<32>;

    //Shader variant key caching
    struct ShaderVariantKey
    {
        MaterialType materialType;
        ShaderFeatureFlags features;
        std::string vertexLayoutSignature;

        bool operator==(const ShaderVariantKey& other) const
        {
            return materialType == other.materialType &&
                features == other.features &&
                vertexLayoutSignature == other.vertexLayoutSignature;
        }
        std::string ToString()const
        {
            return std::to_string(static_cast<int>(materialType)) + "_" +
                std::to_string(features.to_ulong()) + "_" + vertexLayoutSignature;
        }
    };

    struct ShaderVariantKeyHash {
        size_t operator()(const ShaderVariantKey& key) const {
            size_t h1 = std::hash<int>{}(static_cast<int>(key.materialType));
            size_t h2 = std::hash<unsigned long>{}(key.features.to_ulong());
            size_t h3 = std::hash<std::string>{}(key.vertexLayoutSignature);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    class IntegratedShaderManager
    {
    public:
        static IntegratedShaderManager& Instance()
        {
            static IntegratedShaderManager instance;
            return instance;
        }

        void Initialize();
        void Shutdown();

        //get shader variat that matches mesh layout and material
        std::shared_ptr<ShaderProgram> GetShaderVariant(const VertexLayout& layout,
            const Material* material,
            MaterialType materialType
           );

        //precompile common Variants at startup
        void PrecompileCommonVariants();

        void ClearVariantCache() { m_VariantCache.clear(); }
    private:
        IntegratedShaderManager() = default;
        
        //generate features form vertex layout and material
        ShaderFeatureFlags GenerateFeatures(const VertexLayout& layout, const Material* material);

        //Create Vertex layout signature for caching 
        std::string CreateLayoutSignature(const VertexLayout& layout);


        //Build Shader defines string for compilation
        std::string BuildDefinesString(const ShaderFeatureFlags& features, const VertexLayout& layout);

        // Get base Shader paths for material Types
        std::pair<std::string, std::string> GetBaseShaderPaths(MaterialType type);
        
        //compile Shaders with defines
        Microsoft::WRL::ComPtr<ID3DBlob> CompileShaderWithDefines(
            const std::string& filePath,
            const std::string& defines,
            const std::string& target,
            const std::string& entryPoint = "main");
        
        //Create Shader variant form compiled blobs
        std::shared_ptr<ShaderProgram> CreateShaderVariant(
            const ShaderVariantKey& key,
            const VertexLayout& layout
        );

        std::unordered_map<ShaderVariantKey, std::shared_ptr<ShaderProgram>, ShaderVariantKeyHash> m_VariantCache;
        std::string m_ShaderBasePath = "assets/Shaders/";
	};


    //utility funtions 
    ShaderFeatureFlags AnalyzeVertexLayout(const VertexLayout& layout);
    ShaderFeatureFlags AnalyzeMaterial(const Material* material);
    bool IsCompatibleWithStandardInput(const VertexLayout& layout);
    std::string GenerateCustomVertexInputHLSL(const VertexLayout& layout);
    std::string GetHLSLType(DataFormat format);

}

