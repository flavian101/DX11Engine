#include "dxpch.h"
#include "ShaderVariantManager.h"
#include "utils/material/Material.h"
#include "utils/VertexShader.h"
#include "utils/PixelShader.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <codecvt>

namespace DXEngine
{

	bool ShaderVariantManager::Initialize(const ShaderVariantConfig& config)
	{
		if (m_Initialized)
		{
			LogWarning("Shader Variant Manager already initialized");
			return true;
		}
		m_Config = config;
		m_Stats.Reset();
		
		// Verify shader directory exists
		if (!std::filesystem::exists(m_Config.shaderBasePath)) {
			LogError("Shader directory does not exist: " + m_Config.shaderBasePath);
			return false;
		}

		// Setup common vertex layouts for precompilation
		m_CommonLayouts = {
			VertexLayout::CreateBasic(),    // Position, Normal, TexCoord
			VertexLayout::CreateLit(),      // + Tangent
			VertexLayout::CreateSkinned(),  // + Blend data
			VertexLayout::CreateUI()        // UI specific
		};

		//precompile common Variants if Requested
		if (m_Config.precompileCommonVariants)
		{
			PrecompileCommonVariants();
		}

		m_Initialized = true;
		LogInfo("ShaderVariantManager initialized successfully");
		return true;
	}
	void ShaderVariantManager::Shutdown()
	{
		if (!m_Initialized) return;

		std::lock_guard<std::mutex> lock(m_CacheMutex);

		m_VariantCache.clear();
		m_VariantUsage.clear();
		m_FileTimestamps.clear();
		m_TrackedFiles.clear();

		m_Initialized = false;
		LogInfo("ShaderVariantManager shutdown complete");

	}
	void ShaderVariantManager::Update()
	{
		if (!m_Initialized)return;
		m_CurrentFrame++;

		//check for hot realod periodicaly(every 60 frames)
		if (m_Config.enableHotReload && (m_CurrentFrame % 60 == 0)) {
			CheckForFileChanges();
		}
	}
	std::shared_ptr<ShaderProgram> ShaderVariantManager::GetShaderVariant(const VertexLayout& layout, const Material* material, MaterialType materialType)
	{

		if (!m_Initialized) {
			LogError("ShaderVariantManager not initialized");
			return nullptr;
		}

		// Generate variant key
		ShaderVariantKey key;
		key.materialType = materialType;
		key.vertexLayoutHash = GenerateVertexLayoutHash(layout);

		// Analyze features
		ShaderFeatureFlags layoutFeatures = AnalyzeVertexLayout(layout);
		ShaderFeatureFlags materialFeatures = AnalyzeMaterial(material);
		key.features = CombineFeatures(layoutFeatures, materialFeatures, materialType);

		return GetShaderVariant(key);
	}
	std::shared_ptr<ShaderProgram> ShaderVariantManager::GetShaderVariant(const ShaderVariantKey& key)
	{
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		// Check cache first
		auto it = m_VariantCache.find(key);
		if (it != m_VariantCache.end()) {
			m_Stats.cacheHits++;
			m_VariantUsage[key] = m_CurrentFrame; // Update usage
			return it->second;
		}

		m_Stats.cacheMisses++;

		// Create new variant
//		auto variant = CreateShaderVariant(key,);
//		if (variant) {
//			m_VariantCache[key] = variant;
//			m_VariantUsage[key] = m_CurrentFrame;
//			m_Stats.totalVariants++;
//
//#ifdef _DEBUG
//			LogInfo("Created shader variant: " + key.ToString());
//#endif
//		}
//		else {
//			m_Stats.compilationFailures++;
//			LogError("Failed to create shader variant: " + key.ToString());
//		}
		LogError("Failed to Find shader variant: " + key.ToString());
		return nullptr;
	}
	void ShaderVariantManager::PrecompileCommonVariants()
	{
		LogInfo("Precompiling common shader variants...");

		std::vector<MaterialType> commonMaterialTypes = {
			MaterialType::Lit,
			MaterialType::Unlit,
			MaterialType::Transparent,
			MaterialType::UI
		};
		size_t variantsCompiled = 0;

		for (const auto& layout : m_CommonLayouts) {
			for (MaterialType materialType : commonMaterialTypes) {
				// Skip incompatible combinations
				if (materialType == MaterialType::UI &&
					layout.GetDebugString().find("UI") == std::string::npos) {
					continue;
				}

				ShaderVariantKey key;
				key.materialType = materialType;
				key.vertexLayoutHash = GenerateVertexLayoutHash(layout);
				key.features = CombineFeatures(
					AnalyzeVertexLayout(layout),
					ShaderFeatureFlags{}, // No material-specific features for precompilation
					materialType
				);

				if (CreateShaderVariant(key,layout)) {
					variantsCompiled++;
				}
			}
		}

		LogInfo("Precompiled " + std::to_string(variantsCompiled) + " shader variants");
	}

	std::shared_ptr<ShaderProgram> ShaderVariantManager::CreateShaderVariant(const ShaderVariantKey& key, const VertexLayout& layout)
	{
		// Get shader file paths
		auto [vsPath, psPath] = GetShaderPaths(key.materialType);

		if (!std::filesystem::exists(vsPath) || !std::filesystem::exists(psPath)) {
			LogError("Shader files not found: " + vsPath + " or " + psPath);

			// Try fallback shaders
			vsPath = m_Config.shaderBasePath + m_Config.fallbackVertexShader;
			psPath = m_Config.shaderBasePath + m_Config.fallbackPixelShader;

			if (!std::filesystem::exists(vsPath) || !std::filesystem::exists(psPath)) {
				LogError("Fallback shader files not found");
				return nullptr;
			}
		}

		// Generate defines string
		std::string defines = GenerateDefinesString(key.features,layout);

		// Compile shaders
		auto vsBlob = CompileShader(vsPath, defines, "vs_5_0", "main");
		auto psBlob = CompileShader(psPath, defines, "ps_5_0", "main");

		if (!vsBlob || !psBlob) {
			return nullptr;
		}

		try {
			// Create shader objects
			auto vertexShader = std::make_shared<VertexShader>(vsBlob.Get());
			auto pixelShader = std::make_shared<PixelShader>(psBlob.Get());

			// Create shader program
			auto program = std::make_shared<ShaderProgram>(vertexShader, pixelShader);

			// Track files for hot reload
			if (m_Config.enableHotReload) {
				UpdateFileTimestamp(vsPath);
				UpdateFileTimestamp(psPath);
			}

			return program;
		}
		catch (const std::exception& e) {
			LogError("Failed to create shader program: " + std::string(e.what()));
			return nullptr;
		}
	}

	Microsoft::WRL::ComPtr<ID3DBlob> ShaderVariantManager::CompileShader(const std::string& filePath, const std::string& defines, const std::string& target, const std::string& entryPoint)
	{
		// Read shader file
		std::ifstream file(filePath);
		if (!file.is_open()) {
			LogError("Failed to open shader file: " + filePath);
			return nullptr;
		}

		std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		file.close();

		// Prepend defines
		std::string finalSource = defines + "\n" + source;

		// Setup compilation flags
		DWORD shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

		if (m_Config.enableDebugInfo) {
			shaderFlags |= D3DCOMPILE_DEBUG;
			shaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		}
		else if (m_Config.enableOptimization) {
			shaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
		}

		Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

		HRESULT hr = D3DCompile(
			finalSource.c_str(),
			finalSource.size(),
			filePath.c_str(),
			nullptr, // Additional defines
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
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
			LogError(error);
			return nullptr;
		}

		return shaderBlob;
	}
	void ShaderVariantManager::PrecompileVariant(const ShaderVariantKey& key)
	{
	}
	void ShaderVariantManager::ClearCache()
	{
		std::lock_guard<std::mutex> lock(m_CacheMutex);
		m_VariantCache.clear();
		m_VariantUsage.clear();
		m_Stats.totalVariants = 0;
		LogInfo("Shader variant cache cleared");
	}
	void ShaderVariantManager::PruneLeastUsedVariants(size_t maxVariants)
	{
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		if (m_VariantCache.size() <= maxVariants) {
			return; // No pruning needed
		}

		// Create vector of (key, last_used_frame) pairs
		std::vector<std::pair<ShaderVariantKey, size_t>> usageVector;
		for (const auto& [key, usage] : m_VariantUsage) {
			usageVector.emplace_back(key, usage);
		}

		// Sort by usage (oldest first)
		std::sort(usageVector.begin(), usageVector.end(),
			[](const auto& a, const auto& b) { return a.second < b.second; });

		// Remove least recently used variants
		size_t toRemove = m_VariantCache.size() - maxVariants;
		for (size_t i = 0; i < toRemove && i < usageVector.size(); ++i) {
			const auto& key = usageVector[i].first;
			m_VariantCache.erase(key);
			m_VariantUsage.erase(key);
			m_Stats.totalVariants--;
		}

		LogInfo("Pruned " + std::to_string(toRemove) + " shader variants from cache");

	}
	void ShaderVariantManager::EnableHotReload(bool enable)
	{
		m_Config.enableHotReload = enable;
		LogInfo("Hot reload " + std::string(enable ? "enabled" : "disabled"));

	}
	void ShaderVariantManager::ReloadShader(const std::string& shaderPath)
	{
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		// Clear all variants that use this shader
		auto it = m_VariantCache.begin();
		while (it != m_VariantCache.end()) {
			auto [vsPath, psPath] = GetShaderPaths(it->first.materialType);
			if (vsPath == shaderPath || psPath == shaderPath) {
				it = m_VariantCache.erase(it);
				m_Stats.hotReloads++;
			}
			else {
				++it;
			}
		}

		UpdateFileTimestamp(shaderPath);
		LogInfo("Reloaded shader: " + shaderPath);
	}
	void ShaderVariantManager::ReloadAllShaders()
	{
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		size_t reloadedCount = m_VariantCache.size();
		m_VariantCache.clear();
		m_VariantUsage.clear();
		m_Stats.totalVariants = 0;
		m_Stats.hotReloads += reloadedCount;

		// Update all file timestamps
		for (const auto& filePath : m_TrackedFiles) {
			UpdateFileTimestamp(filePath);
		}

		LogInfo("Reloaded all shader variants (" + std::to_string(reloadedCount) + " variants)");

	}
	std::vector<std::string> ShaderVariantManager::GetLoadedVariantNames() const
	{
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		std::vector<std::string> names;
		names.reserve(m_VariantCache.size());

		for (const auto& [key, shader] : m_VariantCache) {
			names.push_back(key.ToString());
		}

		return names;
	}
	std::string ShaderVariantManager::GetDebugInfo() const
	{
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		std::ostringstream info;
		info << "=== Shader Variant Manager Debug Info ===\n";
		info << "Initialized: " << (m_Initialized ? "Yes" : "No") << "\n";
		info << "Shader Base Path: " << m_Config.shaderBasePath << "\n";
		info << "Hot Reload: " << (m_Config.enableHotReload ? "Enabled" : "Disabled") << "\n";
		info << "\n" << m_Stats.ToString();
		info << "\nLoaded Variants:\n";

		for (const auto& [key, shader] : m_VariantCache) {
			info << "  - " << MaterialTypeToString(key.materialType)
				<< " [Features: " << key.features.to_string() << "]\n";
		}

		info << "\nTracked Files (" << m_TrackedFiles.size() << "):\n";
		for (const auto& file : m_TrackedFiles) {
			info << "  - " << file << "\n";
		}

		return info.str();
	}


	ShaderFeatureFlags ShaderVariantManager::AnalyzeVertexLayout(const VertexLayout& layout)
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
	ShaderFeatureFlags ShaderVariantManager::AnalyzeMaterial(const Material* material)
	{
		ShaderFeatureFlags features;

		if (!material) return features;

		// Texture features
		if (material->HasFlag(MaterialFlags::HasDiffuseTexture))
			features.set(static_cast<size_t>(ShaderFeature::HasDiffuseTexture));
		if (material->HasFlag(MaterialFlags::HasNormalMap))
			features.set(static_cast<size_t>(ShaderFeature::HasNormalMap));
		if (material->HasFlag(MaterialFlags::HasSpecularMap))
			features.set(static_cast<size_t>(ShaderFeature::HasSpecularMap));
		if (material->HasFlag(MaterialFlags::HasEmissiveMap))
			features.set(static_cast<size_t>(ShaderFeature::HasEmissiveMap));

		// Rendering features
		if (material->HasFlag(MaterialFlags::ReceivesShadows))
			features.set(static_cast<size_t>(ShaderFeature::EnableShadows));

		return features;
	}
	ShaderFeatureFlags ShaderVariantManager::CombineFeatures(const ShaderFeatureFlags& layoutFeatures, const ShaderFeatureFlags& materialFeatures, MaterialType materialType)
	{
		ShaderFeatureFlags combined = layoutFeatures | materialFeatures;

		// Add material type specific features
		switch (materialType) {
		case MaterialType::Transparent:
			combined.set(static_cast<size_t>(ShaderFeature::EnableAlphaTest));
			break;
		case MaterialType::Emissive:
			combined.set(static_cast<size_t>(ShaderFeature::EnableEmissive));
			break;
		case MaterialType::Skybox:
			combined.set(static_cast<size_t>(ShaderFeature::HasEnvironmentMap));
			break;
		}

		return combined;
	}
	std::pair<std::string, std::string> ShaderVariantManager::GetShaderPaths(MaterialType materialType)
	{
		std::string basePath = m_Config.shaderBasePath;


		switch (materialType) {
		case MaterialType::Lit:
			return { basePath + "Lit.vs.hlsl", basePath + "Lit.ps.hlsl" };
		case MaterialType::Unlit:
			return { basePath + "Unlit.vs.hlsl", basePath + "Unlit.ps.hlsl" };
		case MaterialType::Transparent:
			return { basePath + "Transparent.vs.hlsl", basePath + "Transparent.ps.hlsl" };
		case MaterialType::Emissive:
			return { basePath + "Emissive.vs.hlsl", basePath + "Emissive.ps.hlsl" };
		case MaterialType::Skybox:
			return { basePath + "Skybox.vs.hlsl", basePath + "Skybox.ps.hlsl" };
		case MaterialType::UI:
			return { basePath + "UI.vs.hlsl", basePath + "UI.ps.hlsl" };
		default:
			return { basePath + "Lit.vs.hlsl", basePath + "Lit.ps.hlsl" };
		}
	}
	std::string ShaderVariantManager::GenerateDefinesString(const ShaderFeatureFlags& features, const VertexLayout& layout)
	{
		std::ostringstream defines;

		// Texture features
		if (features.test(static_cast<size_t>(ShaderFeature::HasDiffuseTexture)))
			defines << "#define HAS_DIFFUSE_TEXTURE 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::HasNormalMap)))
			defines << "#define HAS_NORMAL_MAP 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::HasSpecularMap)))
			defines << "#define HAS_SPECULAR_MAP 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::HasEmissiveMap)))
			defines << "#define HAS_EMISSIVE_MAP 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::HasEnvironmentMap)))
			defines << "#define HAS_ENVIRONMENT_MAP 1\n";

		// Vertex attribute features
		if (features.test(static_cast<size_t>(ShaderFeature::HasTangent)))
			defines << "#define HAS_TANGENT_ATTRIBUTE 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::HasVertexColor)))
			defines << "#define HAS_VERTEX_COLOR_ATTRIBUTE 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::HasSecondUV)))
			defines << "#define HAS_SECOND_UV_ATTRIBUTE 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::HasBlendWeights)))
			defines << "#define HAS_SKINNING_ATTRIBUTES 1\n";

		// Rendering features
		if (features.test(static_cast<size_t>(ShaderFeature::EnableShadows)))
			defines << "#define ENABLE_SHADOWS 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::EnableFog)))
			defines << "#define ENABLE_FOG 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::EnableInstancing)))
			defines << "#define ENABLE_INSTANCING 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::EnableAlphaTest)))
			defines << "#define ENABLE_ALPHA_TEST 1\n";
		if (features.test(static_cast<size_t>(ShaderFeature::EnableEmissive)))
			defines << "#define ENABLE_EMISSIVE 1\n";

		return defines.str();
	}
	std::string ShaderVariantManager::GenerateVertexLayoutHash(const VertexLayout& layout)
	{
		std::string hash;
		for (const auto& attr : layout.GetAttributes()) {
			hash += std::to_string(static_cast<int>(attr.Type));
			hash += std::to_string(static_cast<int>(attr.Format));
			hash += std::to_string(attr.Slot);
			hash += "_";
		}
		return hash;
	}
	void ShaderVariantManager::CheckForFileChanges()
	{
		bool needsReload = false;
		std::vector<std::string> changedFiles;

		for (const auto& [filePath, storedTime] : m_FileTimestamps) {
			if (HasFileChanged(filePath)) {
				needsReload = true;
				changedFiles.push_back(filePath);
			}
		}

		if (needsReload) {
			LogInfo("Hot reloading shaders due to file changes");
			for (const auto& file : changedFiles) {
				ReloadShader(file);
			}
		}
	}
	void ShaderVariantManager::UpdateFileTimestamp(const std::string& filePath)
	{
		try {
			if (std::filesystem::exists(filePath)) {
				auto time = std::filesystem::last_write_time(filePath);
				m_FileTimestamps[filePath] =
					std::chrono::duration_cast<std::chrono::milliseconds>(
						time.time_since_epoch()).count();

				// Add to tracked files if not already tracked
				if (std::find(m_TrackedFiles.begin(), m_TrackedFiles.end(), filePath) == m_TrackedFiles.end()) {
					m_TrackedFiles.push_back(filePath);
				}
			}
		}
		catch (...) {
			// Ignore filesystem errors
		}
	}
	bool ShaderVariantManager::HasFileChanged(const std::string& filePath)
	{
		try {
			if (!std::filesystem::exists(filePath)) {
				return false;
			}

			auto currentTime = std::filesystem::last_write_time(filePath);
			auto currentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
				currentTime.time_since_epoch()).count();

			auto it = m_FileTimestamps.find(filePath);
			return it != m_FileTimestamps.end() && it->second != currentTimeMs;
		}
		catch (...) {
			return false;
		}
	}
	std::string ShaderVariantManager::MaterialTypeToString(MaterialType type) const
	{
		switch (type) {
		case MaterialType::Lit: return "Lit";
		case MaterialType::Unlit: return "Unlit";
		case MaterialType::Transparent: return "Transparent";
		case MaterialType::Emissive: return "Emissive";
		case MaterialType::Skybox: return "Skybox";
		case MaterialType::UI: return "UI";
		default: return "Unknown";
		}
	}
	std::wstring ShaderVariantManager::StringToWString(const std::string& str)
	{
		if (str.empty()) return std::wstring();

		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		return converter.from_bytes(str);
	}
	void ShaderVariantManager::LogError(const std::string& message)
	{
		OutputDebugStringA(("[ShaderVariantManager ERROR] " + message + "\n").c_str());

	}
	void ShaderVariantManager::LogWarning(const std::string& message)
	{
		OutputDebugStringA(("[ShaderVariantManager WARNING] " + message + "\n").c_str());

	}
	void ShaderVariantManager::LogInfo(const std::string& message)
	{
		OutputDebugStringA(("[ShaderVariantManager INFO] " + message + "\n").c_str());

	}

	namespace ShaderVariantUtils {

		ShaderFeatureFlags CreateFeatureFlags(const std::vector<ShaderFeature>& features) {
			ShaderFeatureFlags flags;
			for (ShaderFeature feature : features) {
				flags.set(static_cast<size_t>(feature));
			}
			return flags;
		}

		bool HasFeature(const ShaderFeatureFlags& flags, ShaderFeature feature) {
			return flags.test(static_cast<size_t>(feature));
		}

		void SetFeature(ShaderFeatureFlags& flags, ShaderFeature feature, bool enabled) {
			flags.set(static_cast<size_t>(feature), enabled);
		}

		std::string FeaturesToString(const ShaderFeatureFlags& flags) {
			std::vector<std::string> featureNames;

			if (HasFeature(flags, ShaderFeature::HasDiffuseTexture)) featureNames.push_back("DiffuseTex");
			if (HasFeature(flags, ShaderFeature::HasNormalMap)) featureNames.push_back("NormalMap");
			if (HasFeature(flags, ShaderFeature::HasSpecularMap)) featureNames.push_back("SpecularMap");
			if (HasFeature(flags, ShaderFeature::HasEmissiveMap)) featureNames.push_back("EmissiveMap");
			if (HasFeature(flags, ShaderFeature::HasEnvironmentMap)) featureNames.push_back("EnvMap");

			if (HasFeature(flags, ShaderFeature::HasTangent)) featureNames.push_back("Tangent");
			if (HasFeature(flags, ShaderFeature::HasVertexColor)) featureNames.push_back("VertexColor");
			if (HasFeature(flags, ShaderFeature::HasBlendWeights)) featureNames.push_back("Skinning");
			if (HasFeature(flags, ShaderFeature::HasSecondUV)) featureNames.push_back("SecondUV");

			if (HasFeature(flags, ShaderFeature::EnableShadows)) featureNames.push_back("Shadows");
			if (HasFeature(flags, ShaderFeature::EnableFog)) featureNames.push_back("Fog");
			if (HasFeature(flags, ShaderFeature::EnableInstancing)) featureNames.push_back("Instancing");
			if (HasFeature(flags, ShaderFeature::EnableAlphaTest)) featureNames.push_back("AlphaTest");
			if (HasFeature(flags, ShaderFeature::EnableEmissive)) featureNames.push_back("Emissive");

			if (featureNames.empty()) {
				return "None";
			}

			std::string result;
			for (size_t i = 0; i < featureNames.size(); ++i) {
				if (i > 0) result += ", ";
				result += featureNames[i];
			}
			return result;
		}

		bool IsStandardLayout(const VertexLayout& layout) {
			// Check if layout matches standard vertex input
			return layout.HasAttribute(VertexAttributeType::Position) &&
				layout.HasAttribute(VertexAttributeType::Normal) &&
				layout.HasAttribute(VertexAttributeType::TexCoord0) &&
				layout.HasAttribute(VertexAttributeType::Tangent) &&
				layout.GetAttributeCount() == 4;
		}

		std::string GetLayoutDescription(const VertexLayout& layout) {
			std::vector<std::string> attributes;

			for (const auto& attr : layout.GetAttributes()) {
				std::string name = GetSemanticName(attr.Type);
				std::string format = GetHLSLDataType(attr.Format);
				attributes.push_back(name + "(" + format + ")");
			}

			std::string result;
			for (size_t i = 0; i < attributes.size(); ++i) {
				if (i > 0) result += ", ";
				result += attributes[i];
			}
			return result;
		}

		ShaderFeatureFlags ExtractMaterialFeatures(const Material* material) {
			ShaderFeatureFlags features;

			if (!material) return features;

			if (material->HasFlag(MaterialFlags::HasDiffuseTexture))
				SetFeature(features, ShaderFeature::HasDiffuseTexture, true);
			if (material->HasFlag(MaterialFlags::HasNormalMap))
				SetFeature(features, ShaderFeature::HasNormalMap, true);
			if (material->HasFlag(MaterialFlags::HasSpecularMap))
				SetFeature(features, ShaderFeature::HasSpecularMap, true);
			if (material->HasFlag(MaterialFlags::HasEmissiveMap))
				SetFeature(features, ShaderFeature::HasEmissiveMap, true);
			if (material->HasFlag(MaterialFlags::ReceivesShadows))
				SetFeature(features, ShaderFeature::EnableShadows, true);

			return features;
		}

		MaterialType DeduceMaterialType(const Material* material) {
			if (!material) return MaterialType::Lit;
			return material->GetType();
		}

		std::string GenerateVertexInputStructure(const VertexLayout& layout) {
			std::ostringstream hlsl;

			hlsl << "struct CustomVertexInput {\n";

			for (const auto& attr : layout.GetAttributes()) {
				std::string hlslType = GetHLSLDataType(attr.Format);
				std::string semantic = GetSemanticName(attr.Type);
				std::string memberName = semantic;
				std::transform(memberName.begin(), memberName.end(), memberName.begin(), ::tolower);

				hlsl << "    " << hlslType << " " << memberName << " : " << semantic;

				// Add index for multi-channel semantics
				if (attr.Type == VertexAttributeType::TexCoord1) {
					hlsl << "1";
				}
				else if (attr.Type == VertexAttributeType::Color0) {
					hlsl << "0";
				}

				hlsl << ";\n";
			}

			hlsl << "};\n";
			return hlsl.str();
		}

		std::string GetHLSLDataType(DataFormat format) {
			switch (format) {
			case DataFormat::Float: return "float";
			case DataFormat::Float2: return "float2";
			case DataFormat::Float3: return "float3";
			case DataFormat::Float4: return "float4";
			case DataFormat::Int: return "int";
			case DataFormat::Int2: return "int2";
			case DataFormat::Int3: return "int3";
			case DataFormat::Int4: return "int4";
			case DataFormat::UByte4: return "uint4";
			case DataFormat::UByte4N: return "float4";
			case DataFormat::Short2: return "int2";
			case DataFormat::Short2N: return "float2";
			case DataFormat::Short4: return "int4";
			case DataFormat::Short4N: return "float4";
			case DataFormat::Half2: return "half2";
			case DataFormat::Half4: return "half4";
			default: return "float4";
			}
		}

		std::string GetSemanticName(VertexAttributeType type) {
			switch (type) {
			case VertexAttributeType::Position: return "POSITION";
			case VertexAttributeType::Normal: return "NORMAL";
			case VertexAttributeType::Tangent: return "TANGENT";
			case VertexAttributeType::TexCoord0: return "TEXCOORD";
			case VertexAttributeType::TexCoord1: return "TEXCOORD";
			case VertexAttributeType::Color0: return "COLOR";
			case VertexAttributeType::BlendIndices: return "BLENDINDICES";
			case VertexAttributeType::BlendWeights: return "BLENDWEIGHT";
			default: return "UNKNOWN";
			}
		}
	}
}