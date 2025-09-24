#include "dxpch.h"
#include "ShaderVariant.h"
#include "utils/material/Material.h"
#include "shaders/ShaderProgram.h"
#include <sstream>
#include <fstream>
#include <filesystem>

namespace DXEngine
{
    void IntegratedShaderManager::Initialize()
    {
        OutputDebugStringA("IntegratedShaderManager: Initializing\n");

        //verify shader files exists
        std::vector<std::string> requiredShaders = {
        "common.hlsli", "Lit.vs.hlsl", "Lit.ps.hlsl",
        "Unlit.vs.hlsl", "Unlit.ps.hlsl", "UI.vs.hlsl", "UI.ps.hlsl",
        "Transparent.vs.hlsl", "Transparent.ps.hlsl",
        "Emissive.vs.hlsl", "Emissive.ps.hlsl",
        "Skybox.vs.hlsl", "Skybox.ps.hlsl"
        };

        for (const auto& shader : requiredShaders) {
            std::string fullPath = m_ShaderBasePath + shader;
            if (!std::filesystem::exists(fullPath)) {
                OutputDebugStringA(("Warning: Missing shader file: " + fullPath + "\n").c_str());
            }
        }

        PrecompileCommonVariants();

    }
    void IntegratedShaderManager::Shutdown()
    {
        m_VariantCache.clear();
#ifdef DEBUG
        OutputDebugStringA("InteratedShaderManager: shutdwn comleted\n");
#endif 

    }
    std::shared_ptr<ShaderProgram> IntegratedShaderManager::GetShaderVariant(const VertexLayout& layout, const Material* material, MaterialType materialType)
    {
        //generate variant Key 
        ShaderVariantKey key;
        key.materialType = materialType;
        key.features = GenerateFeatures(layout, material);
        key.vertexLayoutSignature = CreateLayoutSignature(layout);

        //check cache first
       auto it = m_VariantCache.find(key);
        if (it != m_VariantCache.end()) {
            return it->second;
        }

        //create new Variant 
        auto variant = CreateShaderVariant(key, layout);
        if (variant)
        {
            m_VariantCache[key] = variant;
#ifdef _DEBUG
            std::string debugMsg = "Created shader variant: " + key.ToString() + "\n";
            OutputDebugStringA(debugMsg.c_str());
#endif
        }


        return variant;
    }
    void IntegratedShaderManager::PrecompileCommonVariants()
    {
        std::vector<VertexLayout> commonLayouts =
        {
            VertexLayout::CreateBasic(),     // Position + Normal + TexCoord
            VertexLayout::CreateLit(),       // + Tangent
            VertexLayout::CreateUI(),        // UI elements
            VertexLayout::CreateSkinned()    // + Blend data
        };
        std::vector<MaterialType> commonTypes = {
            MaterialType::Lit,
            MaterialType::Unlit,
            MaterialType::UI
        };

        size_t variantsCompiled = 0;
        for (const auto& layout : commonLayouts)
        {
            for (const auto& type : commonTypes) {
                // Skip incompatible combinations
                if (type == MaterialType::UI && layout.GetDebugString().find("UI") == std::string::npos)
                    continue;

                auto variant = GetShaderVariant(layout, nullptr, type);
                if (variant) {
                    variantsCompiled++;
                }
            }
        }
        std::string msg = "Precompiled " + std::to_string(variantsCompiled) + " shader variants\n";
        OutputDebugStringA(msg.c_str());

    }
    ShaderFeatureFlags IntegratedShaderManager::GenerateFeatures(const VertexLayout& layout, const Material* material)
    {
        ShaderFeatureFlags features;
        //analyze vertex layout
        features |= AnalyzeVertexLayout(layout);

        //Analyze material
        if (material)
        {
            features |= AnalyzeMaterial(material);
        }

        return features;
    }
    std::string IntegratedShaderManager::CreateLayoutSignature(const VertexLayout& layout)
    {
        std::string signature;
        for (const auto& attr : layout.GetAttributes())
        {
            signature += std::to_string(static_cast<int>(attr.Type));
            signature += std::to_string(static_cast<int>(attr.Format));
            signature += std::to_string(attr.Slot);
            signature += "_";
        }
        return signature;
    }
    std::string IntegratedShaderManager::BuildDefinesString(const ShaderFeatureFlags& features, const VertexLayout& layout)
    {
        std::ostringstream defines;

        // Add feature defines that your shaders already check for
        if (features.test(static_cast<size_t>(ShaderFeature::HasDiffuseTexture)))
            defines << "#define HAS_DIFFUSE_TEXTURE 1\n";
        if (features.test(static_cast<size_t>(ShaderFeature::HasNormalMap)))
            defines << "#define HAS_NORMAL_MAP 1\n";
        if (features.test(static_cast<size_t>(ShaderFeature::HasSpecularMap)))
            defines << "#define HAS_SPECULAR_MAP 1\n";
        if (features.test(static_cast<size_t>(ShaderFeature::HasEmissiveMap)))
            defines << "#define HAS_EMISSIVE_MAP 1\n";
        if (features.test(static_cast<size_t>(ShaderFeature::EnableShadows)))
            defines << "#define ENABLE_SHADOWS 1\n";

        // Add custom defines for vertex layout features
        if (features.test(static_cast<size_t>(ShaderFeature::HasTangent)))
            defines << "#define HAS_TANGENT_ATTRIBUTE 1\n";
        if (features.test(static_cast<size_t>(ShaderFeature::HasVertexColor)))
            defines << "#define HAS_VERTEX_COLOR_ATTRIBUTE 1\n";
        if (features.test(static_cast<size_t>(ShaderFeature::HasSecondUV)))
            defines << "#define HAS_SECOND_UV_ATTRIBUTE 1\n";
        if (features.test(static_cast<size_t>(ShaderFeature::HasBlendWeights)))
            defines << "#define HAS_SKINNING_ATTRIBUTES 1\n";

        // Generate custom vertex input structure if needed
        if (!IsCompatibleWithStandardInput(layout)) {
            defines << "#define CUSTOM_VERTEX_INPUT 1\n";
            defines << GenerateCustomVertexInputHLSL(layout);
        }

        return defines.str();
    }
    std::pair<std::string, std::string> IntegratedShaderManager::GetBaseShaderPaths(MaterialType type)
    {
        switch (type) {
        case MaterialType::Lit:
            return { m_ShaderBasePath + "Lit.vs.hlsl", m_ShaderBasePath + "Lit.ps.hlsl" };

        case MaterialType::Unlit:
            return { m_ShaderBasePath + "Unlit.vs.hlsl", m_ShaderBasePath + "Unlit.ps.hlsl" };

        case MaterialType::Transparent:
            return { m_ShaderBasePath + "Transparent.vs.hlsl", m_ShaderBasePath + "Transparent.ps.hlsl" };

        case MaterialType::Emissive:
            return { m_ShaderBasePath + "Emissive.vs.hlsl", m_ShaderBasePath + "Emissive.ps.hlsl" };

        case MaterialType::Skybox:
            return { m_ShaderBasePath + "Skybox.vs.hlsl", m_ShaderBasePath + "Skybox.ps.hlsl" };

        case MaterialType::UI:
            return { m_ShaderBasePath + "UI.vs.hlsl", m_ShaderBasePath + "UI.ps.hlsl" };

        default:
            return { m_ShaderBasePath + "Lit.vs.hlsl", m_ShaderBasePath + "Lit.ps.hlsl" };
        }
    }
    Microsoft::WRL::ComPtr<ID3DBlob> IntegratedShaderManager::CompileShaderWithDefines(const std::string& filePath, const std::string& defines, const std::string& target, const std::string& entryPoint)
    {
        //read shader File
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            OutputDebugStringA(("Failed to open shader file: " + filePath + "\n").c_str());
            return nullptr;
        }

        std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        //Preapend defines to the source
        std::string finalSource = defines + "\n" + source;

        //compile
        DWORD shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef _DEBUG
        shaderFlags |= D3DCOMPILE_DEBUG;
        shaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        shaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
        Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

        HRESULT hr = D3DCompile(
            finalSource.c_str(),
            finalSource.size(),
            filePath.c_str(),      // Use filename for better error messages
            nullptr,               // No additional defines (already in source)
            D3D_COMPILE_STANDARD_FILE_INCLUDE,  // Support #include
            entryPoint.c_str(),
            target.c_str(),
            shaderFlags,
            0,
            &shaderBlob,
            &errorBlob
        );

        if (FAILED(hr)) {
            std::string error = "Shader compilation failed for: " + filePath + "\n";
            if (errorBlob) {
                error += static_cast<const char*>(errorBlob->GetBufferPointer());
            }
            OutputDebugStringA(error.c_str());
            return nullptr;
        }

        return shaderBlob;
    }
    std::shared_ptr<ShaderProgram> IntegratedShaderManager::CreateShaderVariant(const ShaderVariantKey& key, const VertexLayout& layout)
    {
        //get Shader file paths
        auto [vsPath, psPath] = GetBaseShaderPaths(key.materialType);

        //Build defines strings
        std::string defines = BuildDefinesString(key.features, layout);

        // Compile vertex shader
        auto vsBlob = CompileShaderWithDefines(vsPath, defines, "vs_5_0");
        if (!vsBlob) {
            return nullptr;
        }

        // Compile pixel shader  
        auto psBlob = CompileShaderWithDefines(psPath, defines, "ps_5_0");
        if (!psBlob) {
            return nullptr;
        }

        try{
            // Convert blob to wide string paths (your ShaderProgram constructor needs this)
            std::wstring vsPathW(vsPath.begin(), vsPath.end());
            std::wstring psPathW(psPath.begin(), psPath.end());

            // Create using your existing ShaderProgram constructor but with compiled blobs
            auto vertexShader = std::make_shared<VertexShader>(vsBlob.Get());
            auto pixelShader = std::make_shared<PixelShader>(psBlob.Get());

            //Create using your existing ShaderProgram constructor
            auto shaderProgram = std::make_shared<ShaderProgram>(vertexShader, pixelShader);

            return shaderProgram;
        }
        catch (const std::exception& e) {
            OutputDebugStringA(("Failed (Variant exception) to create shader program: " + std::string(e.what()) + "\n").c_str());
            return nullptr;
        }
    }

    //utility Functions
    ShaderFeatureFlags AnalyzeVertexLayout(const VertexLayout& layout)
    {
        ShaderFeatureFlags features;

        if (layout.HasAttribute(VertexAttributeType::Tangent))
            features.set(static_cast<size_t>(ShaderFeature::HasTangent));
        if (layout.HasAttribute(VertexAttributeType::Color0))
            features.set(static_cast<size_t>(ShaderFeature::HasVertexColor));
        if (layout.HasAttribute(VertexAttributeType::BlendWeights))
            features.set(static_cast<size_t>(ShaderFeature::HasBlendWeights));
        if (layout.HasAttribute(VertexAttributeType::BlendIndices))
            features.set(static_cast<size_t>(ShaderFeature::HasBlendIndices));
        if (layout.HasAttribute(VertexAttributeType::TexCoord1))
            features.set(static_cast<size_t>(ShaderFeature::HasSecondUV));

        return features;
    }
    ShaderFeatureFlags AnalyzeMaterial(const Material* material)
    {
        ShaderFeatureFlags features;

        if (!material) return features;

        // Map your existing material flags to shader features
        if (material->HasFlag(MaterialFlags::HasDiffuseTexture))
            features.set(static_cast<size_t>(ShaderFeature::HasDiffuseTexture));
        if (material->HasFlag(MaterialFlags::HasNormalMap))
            features.set(static_cast<size_t>(ShaderFeature::HasNormalMap));
        if (material->HasFlag(MaterialFlags::HasSpecularMap))
            features.set(static_cast<size_t>(ShaderFeature::HasSpecularMap));
        if (material->HasFlag(MaterialFlags::HasEmissiveMap))
            features.set(static_cast<size_t>(ShaderFeature::HasEmissiveMap));

        // Enable shadows for materials that receive them
        if (material->HasFlag(MaterialFlags::ReceivesShadows))
            features.set(static_cast<size_t>(ShaderFeature::EnableShadows));

        return features;
    }
    bool IsCompatibleWithStandardInput(const VertexLayout& layout)
    {
        // Check if layout matches your StandardVertexInput structure
               // StandardVertexInput has: position, normal, texCoord, tangent

        if (!layout.HasAttribute(VertexAttributeType::Position))
            return false;

        // For most compatibility, require exactly the standard attributes
        bool hasStandardAttributes =
            layout.HasAttribute(VertexAttributeType::Position) &&
            layout.HasAttribute(VertexAttributeType::Normal) &&
            layout.HasAttribute(VertexAttributeType::TexCoord0) &&
            layout.HasAttribute(VertexAttributeType::Tangent);

        // Check if it has ONLY standard attributes (no extras that would break compatibility)
        bool hasOnlyStandard = layout.GetAttributeCount() <= 4;

        return hasStandardAttributes && hasOnlyStandard;
    }
    std::string GenerateCustomVertexInputHLSL(const VertexLayout& layout)
    {
        std::ostringstream hlsl;
        
        hlsl << "// Custom Vertex input structure\n";
        hlsl << "struct CustomVertexinput\n";
            
        for (const auto& attr : layout.GetAttributes())
        {
            std::string hlslType = GetHLSLType(attr.Format);

            switch (attr.Type) {
            case VertexAttributeType::Position:
                hlsl << "    " << hlslType << " position : POSITION;\n";
                break;
            case VertexAttributeType::Normal:
                hlsl << "    " << hlslType << " normal : NORMAL;\n";
                break;
            case VertexAttributeType::Tangent:
                hlsl << "    " << hlslType << " tangent : TANGENT;\n";
                break;
            case VertexAttributeType::TexCoord0:
                hlsl << "    " << hlslType << " texCoord : TEXCOORD0;\n";
                break;
            case VertexAttributeType::TexCoord1:
                hlsl << "    " << hlslType << " texCoord1 : TEXCOORD1;\n";
                break;
            case VertexAttributeType::Color0:
                hlsl << "    " << hlslType << " color : COLOR0;\n";
                break;
            case VertexAttributeType::BlendIndices:
                hlsl << "    " << hlslType << " blendIndices : BLENDINDICES;\n";
                break;
            case VertexAttributeType::BlendWeights:
                hlsl << "    " << hlslType << " blendWeights : BLENDWEIGHT;\n";
                break;
            }
        }

        hlsl << "};\n\n";
        hlsl << "#define StandardVertexInput CustomVertexInput\n";

        return hlsl.str();
    }
    std::string GetHLSLType(DataFormat format)
    {
        switch (format) {
        case DataFormat::Float:     return "float";
        case DataFormat::Float2:    return "float2";
        case DataFormat::Float3:    return "float3";
        case DataFormat::Float4:    return "float4";
        case DataFormat::Int:       return "int";
        case DataFormat::Int2:      return "int2";
        case DataFormat::Int3:      return "int3";
        case DataFormat::Int4:      return "int4";
        case DataFormat::UByte4:    return "uint4";
        case DataFormat::UByte4N:   return "float4";
        case DataFormat::Short2:    return "int2";
        case DataFormat::Short2N:   return "float2";
        case DataFormat::Short4:    return "int4";
        case DataFormat::Short4N:   return "float4";
        case DataFormat::Half2:     return "half2";
        case DataFormat::Half4:     return "half4";
        default:                    return "float4";
        }
    }
}
