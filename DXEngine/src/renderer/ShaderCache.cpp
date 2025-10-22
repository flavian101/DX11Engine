#include "dxpch.h"
#include "ShaderCache.h"
#include <filesystem>
#include <fstream>


namespace DXEngine::Rendering
{
	std::string ShaderCacheStats::ToString() const {
		std::ostringstream oss;
		oss << "Shader Cache Statistics:\n";
		oss << "  Total Variants: " << totalVariants << "\n";
		oss << "  Cache Hit Rate: " << GetHitRate() << "%\n";
		oss << "  Cache Hits: " << cacheHits << "\n";
		oss << "  Cache Misses: " << cacheMisses << "\n";
		oss << "  Compilation Failures: " << compilationFailures << "\n";
		oss << "  Hot Reloads: " << hotReloads << "\n";
		return oss.str();
	}

	ShaderCache::ShaderCache(std::shared_ptr<RHI::IGraphicsDevice> device)
		:m_Device(device)
	{
		if (!m_Device)
		{
			throw std::runtime_error("ShaderCache: Device cannot be null");
		}
	}

	ShaderCache::~ShaderCache()
	{
		Shutdown();
	}

	bool ShaderCache::Initialize(const ShaderCacheConfig& config)
	{
		if (m_Initialized)
		{
			LogWarning("ShaderCache already initialized");
			return true;
		}
		m_Config = config;
		m_Stats.Reset();

		//Verify shader directry exists
		if (!std::filesystem::exists(m_Config.shaderDirectory)) {
			LogError("Shader directory does not exist: " + m_Config.shaderDirectory);
			return false;
		}

		LogInfo("ShaderCache: Initializing...");

		// Load base shaders
		LoadBaseShader("Lit", "Lit.vs.hlsl", "Lit.ps.hlsl");
		LoadBaseShader("PBR", "PBR.vs.hlsl", "PBR.ps.hlsl");
		LoadBaseShader("Unlit", "Unlit.vs.hlsl", "Unlit.ps.hlsl");
		LoadBaseShader("Transparent", "Transparent.vs.hlsl", "Transparent.ps.hlsl");
		LoadBaseShader("Emissive", "Emissive.vs.hlsl", "Emissive.ps.hlsl");
		LoadBaseShader("Skybox", "Skybox.vs.hlsl", "Skybox.ps.hlsl");
		LoadBaseShader("UI", "UI.vs.hlsl", "UI.ps.hlsl");

		//precompile common variants
		if (m_Config.precompileCommonVariants)
		{
			PrecompileCommonVariants();
		}

		m_Initialized = true;
		LogInfo("ShaderCache initialized successfully");
		return true;
	}

	void ShaderCache::Shutdown()
	{
		if (!m_Initialized) return;

		std::lock_guard<std::mutex> lock(m_CacheMutex);

		m_BaseShaders.clear();
		m_VariantCache.clear();
		m_FallbackShaders.clear();
		m_FileTimestamps.clear();

		m_Initialized = false;
		LogInfo("ShaderCache: Shutdown complete");
	}
	
	void ShaderCache::Update()
	{
		if (m_Initialized)
		{
			OutputDebugStringA("ShaderCache: Cannot update");
			return;
		}

		m_CurrentFrame++;

		//check for Hot reload periodically 
		if (m_Config.enableHotReload && (m_CurrentFrame % 60 == 0)) {
			CheckForFileChanges();
		}
	}
	std::shared_ptr<ShaderProgram> ShaderCache::GetShaderForMaterial(const VertexLayout& layout, const Material* material)
	{
		if (!m_Initialized) {
			LogError("ShaderCache not initialized");
			return GetFallbackShader(MaterialType::Lit);
		}

		if (!material) {
			LogWarning("Material is null, using fallback");
			return GetFallbackShader(MaterialType::Lit);
		}

		// Build variant key
		ShaderVariantKey key;
		key.baseName = MaterialTypeToShaderName(material->GetType());
		key.vertexLayoutHash = GenerateVertexLayoutHash(layout);
		key.actualLayout = layout; //Store the actual layout for compilation

		// Analyze features
		uint32_t layoutFeatures = AnalyzeVertexLayout(layout);
		uint32_t materialFeatures = AnalyzeMaterial(material);
		key.featureFlags = CombineFeatures(layoutFeatures, materialFeatures, material->GetType());

		// Try to get cached variant
		auto shader = GetShaderVariant(key);
		if (!shader) {
			LogWarning("Failed to get shader variant, using fallback");
			return GetFallbackShader(material->GetType());
		}

		return shader;
	}

	std::shared_ptr<ShaderProgram> ShaderCache::GetShaderVariant(const ShaderVariantKey& key)
	{
		std::lock_guard<std::mutex> lock(m_CacheMutex);
		auto it = m_VariantCache.find(key);
		if (it != m_VariantCache.end())
		{
			m_Stats.cacheHits++;
			it->second.lastUsedFrame = m_CurrentFrame;
			return it->second.program;
		}

		m_Stats.cacheMisses++;

		//Create a new variant
		auto program = CreateShaderVariant(key);
		if (program)
		{
			VariantEntry entry;
			entry.program = program;
			entry.lastUsedFrame = m_CurrentFrame;

			MaterialType materialType =
				key.baseName == "Lit" ? MaterialType::Lit :
				key.baseName == "PBR" ? MaterialType::PBR :
				key.baseName == "Unlit" ? MaterialType::Unlit :
				key.baseName == "Transparent" ? MaterialType::Transparent :
				key.baseName == "Emissive" ? MaterialType::Emissive :
				key.baseName == "Skybox" ? MaterialType::Skybox :
				key.baseName == "UI" ? MaterialType::UI :
				MaterialType::Lit; // fallback

			auto [vsPath, psPath] = GetShaderPaths(materialType);

			entry.vsPath = vsPath;
			entry.psPath = psPath;
			m_VariantCache[key] = entry;
			m_Stats.totalVariants++;

			LogInfo("Created shader variant: " + key.ToString());
		}
		else {
			m_Stats.compilationFailures++;
			LogError("Failed to create shader variant: " + key.ToString());
		}

		return program;
	}

	std::shared_ptr<ShaderProgram>ShaderCache::GetFallbackShader(MaterialType materialType)
	{
		// Check if we have a fallback for this material type
		auto it = m_FallbackShaders.find(materialType);
		if (it != m_FallbackShaders.end()) {
			return it->second;
		}

		// Create minimal fallback shader
		VertexLayout fallbackLayout;
		fallbackLayout.Position();

		switch (materialType) {
		case MaterialType::UI:
			fallbackLayout.TexCoord(0);
			fallbackLayout.Color(0);
			break;
		case MaterialType::Skybox:
			fallbackLayout.Normal();
			break;
		default:
			fallbackLayout.Normal();
			fallbackLayout.TexCoord(0);
			break;
		}

		fallbackLayout.Finalize();

		ShaderVariantKey key;
		key.baseName = MaterialTypeToShaderName(materialType);
		key.vertexLayoutHash = GenerateVertexLayoutHash(fallbackLayout);
		key.featureFlags = AnalyzeVertexLayout(fallbackLayout);
		key.actualLayout = fallbackLayout;
		auto fallback = CreateShaderVariant(key);
		if (fallback) {
			m_FallbackShaders[materialType] = fallback;
		}

		return fallback;
	}

	bool ShaderCache::LoadBaseShader(const std::string& name, const std::string& vsPath, const std::string& psPath)
	{
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		std::string fullVsPath = m_Config.shaderDirectory + vsPath;
		std::string fullPsPath = m_Config.shaderDirectory + psPath;

		//Compile Shaders without defines
		auto vsShader = CompileShaderFromFile(fullVsPath, RHI::ShaderStage::Vertex);
		auto psShader = CompileShaderFromFile(fullPsPath, RHI::ShaderStage::Pixel);

		if (!vsShader || psShader)
		{
			LogError("Failed to load base shader: " + name);
			return false;
		}
		try
		{
			auto program = std::make_shared<ShaderProgram>(vsShader, psShader);
			ShaderEntry entry;
			entry.program = program;
			entry.vsPath = fullVsPath;
			entry.psPath = fullPsPath;
			entry.lastModified = GetFileModificationTime(fullVsPath);

			m_BaseShaders[name] = entry;
			LogInfo("loaded Base Shader: " + name);
			return true;
		}
		catch (const std::exception& e)
		{
			LogError("Failed to create shader program for " + name + ": " + e.what());
			return false;
		}
	}

	std::shared_ptr<ShaderProgram> ShaderCache::GetBaseShader(const std::string& name) {
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		auto it = m_BaseShaders.find(name);
		if (it != m_BaseShaders.end()) {
			return it->second.program;
		}

		LogWarning("Base shader not found: " + name);
		return nullptr;
	}

	std::shared_ptr<ShaderProgram> ShaderCache::CreateShaderVariant(const ShaderVariantKey& key)
	{
		auto [vsPath, psPath] = GetShaderPaths(
			// Deduce material type from key.baseName
			key.baseName == "Lit" ? MaterialType::Lit :
			key.baseName == "PBR" ? MaterialType::PBR :
			key.baseName == "Unlit" ? MaterialType::Unlit :
			key.baseName == "Transparent" ? MaterialType::Transparent :
			key.baseName == "Emissive" ? MaterialType::Emissive :
			key.baseName == "Skybox" ? MaterialType::Skybox :
			MaterialType::UI
		);

		// Check if shader files exist
		if (!std::filesystem::exists(vsPath) || !std::filesystem::exists(psPath)) {
			LogError("Shader files not found: " + vsPath + " or " + psPath);

			// Try fallback
			vsPath = m_Config.shaderDirectory + m_Config.fallbackVertexShader;
			psPath = m_Config.shaderDirectory + m_Config.fallbackPixelShader;

			if (!std::filesystem::exists(vsPath) || !std::filesystem::exists(psPath)) {
				LogError("Fallback shader files not found");
				return nullptr;
			}
		}

		// Generate defines
		std::string defines = GenerateDefinesString(key.featureFlags, key.actualLayout);

		// Compile with defines
		auto vsShader = CompileShaderFromFile(vsPath, RHI::ShaderStage::Vertex, defines);
		auto psShader = CompileShaderFromFile(psPath, RHI::ShaderStage::Pixel, defines);


		if (!vsShader || !psShader) {
			return nullptr;
		}

		try {
			auto program = std::make_shared<ShaderProgram>(vsShader, psShader);

			// Track for hot reload
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

	std::shared_ptr<RHI::IShader> ShaderCache::CompileShaderFromFile(
		const std::string& path,
		RHI::ShaderStage stage,
		const std::string& defines)
	{
		// Load source
		std::string source = LoadShaderSource(path);
		if (source.empty()) {
			LogError("Failed to load shader source: " + path);
			return nullptr;
		}

		// Prepend defines
		std::string finalSource = defines + "\n" + source;

		//Shader Desc
		RHI::ShaderDesc desc;
		desc.debugName = path;
		desc.sourceCode = finalSource;
		desc.stage = stage;
		desc.entryPoint = "main";

		
		// Set compilation flags based on config
		if (m_Config.enableDebugInfo) {
			desc.enableDebug = true;
			desc.optimizationLevel = RHI::OptimizationLevel::None;
		}
		else if (m_Config.enableOptimization) {
			desc.enableDebug = false;													//TO-DO Improve optimization Selection
			desc.optimizationLevel = RHI::OptimizationLevel::Level3;
		}
		else {
			desc.enableDebug = false;
			desc.optimizationLevel = RHI::OptimizationLevel::Level1;
		}

		//retuns compiled shader
		return m_Device->CreateShader(desc);
	}
	uint32_t ShaderCache::AnalyzeVertexLayout(const VertexLayout& layout) {
		uint32_t features = 0;

		if (layout.HasAttribute(RHI::VertexAttributeType::TexCoord0))
			features |= ShaderFeature::HasTexCoords;
		if (layout.HasAttribute(RHI::VertexAttributeType::Normal))
			features |= ShaderFeature::HasNormals;
		if (layout.HasAttribute(RHI::VertexAttributeType::Tangent))
			features |= ShaderFeature::HasTangents;
		if (layout.HasAttribute(RHI::VertexAttributeType::Color0))
			features |= ShaderFeature::HasVertexColors;
		if (layout.HasAttribute(RHI::VertexAttributeType::BlendWeights))
			features |= ShaderFeature::HasSkinning;
		if (layout.HasAttribute(RHI::VertexAttributeType::TexCoord1))
			features |= ShaderFeature::HasSecondTexCoords;

		return features;
	}

	uint32_t ShaderCache::AnalyzeMaterial(const Material* material) {
		if (!material) return 0;

		uint32_t features = 0;
		uint32_t matFlags = material->GetFeatureFlags();

		// Texture features
		if (matFlags & (1 << 0)) features |= ShaderFeature::DiffuseMap;
		if (matFlags & (1 << 1)) features |= ShaderFeature::NormalMap;
		if (matFlags & (1 << 2)) features |= ShaderFeature::SpecularMap;
		if (matFlags & (1 << 3)) features |= ShaderFeature::EmissiveMap;
		if (matFlags & (1 << 4)) features |= ShaderFeature::EnvironmentMap;
		if (matFlags & (1 << 5)) features |= ShaderFeature::RoughnessMap;
		if (matFlags & (1 << 6)) features |= ShaderFeature::MetallicMap;
		if (matFlags & (1 << 7)) features |= ShaderFeature::AOMap;
		if (matFlags & (1 << 8)) features |= ShaderFeature::HeightMap;
		if (matFlags & (1 << 9)) features |= ShaderFeature::OpacityMap;
		if (matFlags & (1 << 10)) features |= ShaderFeature::DetailDiffuseMap;
		if (matFlags & (1 << 11)) features |= ShaderFeature::DetailNormalMap;
		if (matFlags & (1 << 18)) features |= ShaderFeature::UseDetailsTextures;

		// Rendering features
		if (features & ShaderFeature::HeightMap) {
			features |= ShaderFeature::ParallaxMapping;
		}

		return features;
	}

	uint32_t ShaderCache::CombineFeatures(uint32_t layoutFeatures,
		uint32_t materialFeatures,
		MaterialType materialType)
	{
		uint32_t combined = layoutFeatures | materialFeatures;

		// Add material type specific features
		switch (materialType) {
		case MaterialType::Transparent:
			combined |= ShaderFeature::AlphaTest;
			break;
		case MaterialType::Lit:
		case MaterialType::PBR:
			combined |= ShaderFeature::Lighting;
			break;
		case MaterialType::Skybox:
			combined |= ShaderFeature::EnvironmentMap;
			break;
		}

		return combined;
	}

	// ========== DEFINE STRING GENERATION ==========
	std::string ShaderCache::GenerateDefinesString(uint32_t features, const VertexLayout& layout) {
		std::ostringstream defines;

		// Texture features
		if (features & ShaderFeature::DiffuseMap)
			defines << "#define HAS_DIFFUSE_TEXTURE 1\n";
		if (features & ShaderFeature::NormalMap)
			defines << "#define HAS_NORMAL_MAP 1\n";
		if (features & ShaderFeature::SpecularMap)
			defines << "#define HAS_SPECULAR_MAP 1\n";
		if (features & ShaderFeature::EmissiveMap)
			defines << "#define HAS_EMISSIVE_MAP 1\n";
		if (features & ShaderFeature::EnvironmentMap)
			defines << "#define HAS_ENVIRONMENT_MAP 1\n";
		if (features & ShaderFeature::RoughnessMap)
			defines << "#define HAS_ROUGHNESS_MAP 1\n";
		if (features & ShaderFeature::MetallicMap)
			defines << "#define HAS_METALLIC_MAP 1\n";
		if (features & ShaderFeature::AOMap)
			defines << "#define HAS_AO_MAP 1\n";
		if (features & ShaderFeature::HeightMap)
			defines << "#define HAS_HEIGHT_MAP 1\n";
		if (features & ShaderFeature::OpacityMap)
			defines << "#define HAS_OPACITY_MAP 1\n";
		if (features & ShaderFeature::DetailDiffuseMap)
			defines << "#define HAS_DETAIL_DIFFUSE_MAP 1\n";
		if (features & ShaderFeature::DetailNormalMap)
			defines << "#define HAS_DETAIL_NORMAL_MAP 1\n";
		if (features & ShaderFeature::UseDetailsTextures)
			defines << "#define HAS_DETAIL_TEXTURES 1\n";

		// Vertex attribute features
		if (features & ShaderFeature::HasTexCoords)
			defines << "#define HAS_TEXCOORDS_ATTRIBUTE 1\n";
		if (features & ShaderFeature::HasNormals)
			defines << "#define HAS_NORMAL_ATTRIBUTE 1\n";
		if (features & ShaderFeature::HasTangents)
			defines << "#define HAS_TANGENT_ATTRIBUTE 1\n";
		if (features & ShaderFeature::HasVertexColors)
			defines << "#define HAS_VERTEX_COLOR_ATTRIBUTE 1\n";
		if (features & ShaderFeature::HasSecondTexCoords)
			defines << "#define HAS_SECOND_UV_ATTRIBUTE 1\n";
		if (features & ShaderFeature::HasSkinning)
			defines << "#define HAS_SKINNING_ATTRIBUTES 1\n";

		// Rendering features
		if (features & ShaderFeature::Shadows)
			defines << "#define ENABLE_SHADOWS 1\n";
		if (features & ShaderFeature::Fog)
			defines << "#define ENABLE_FOG 1\n";
		if (features & ShaderFeature::Instancing)
			defines << "#define ENABLE_INSTANCING 1\n";
		if (features & ShaderFeature::AlphaTest)
			defines << "#define ENABLE_ALPHA_TEST 1\n";
		if (features & ShaderFeature::ParallaxMapping)
			defines << "#define ENABLE_PARALLAX_MAPPING 1\n";

		return defines.str();
	}

	void ShaderCache::PrecompileCommonVariants() {
		LogInfo("Precompiling common shader variants...");

		// Common vertex layouts
		std::vector<VertexLayout> layouts = {
			VertexLayout::CreateBasic(),    // Position, Normal, TexCoord
			VertexLayout::CreateLit(),      // + Tangent
			VertexLayout::CreateSkinned(),  // + Blend data
			VertexLayout::CreateUI()        // UI specific
		};

		// Common material types
		std::vector<MaterialType> materialTypes = {
			MaterialType::Lit,
			MaterialType::PBR,
			MaterialType::Unlit,
			MaterialType::Transparent,
			MaterialType::UI
		};

		size_t variantsCompiled = 0;

		for (auto& layout : layouts) {
			layout.Finalize();

			for (MaterialType materialType : materialTypes) {
				// Skip UI shaders for non-UI layouts
				if (materialType == MaterialType::UI &&
					!layout.HasAttribute(RHI::VertexAttributeType::Color0)) {
					continue;
				}

				ShaderVariantKey key;
				key.baseName = MaterialTypeToShaderName(materialType);
				key.vertexLayoutHash = GenerateVertexLayoutHash(layout);
				key.actualLayout = layout;
				key.featureFlags = CombineFeatures(
					AnalyzeVertexLayout(layout),
					0, // No material features for precompilation
					materialType
				);

				if (CreateShaderVariant(key)) {
					variantsCompiled++;
				}
			}
		}

		LogInfo("Precompiled " + std::to_string(variantsCompiled) + " shader variants");
	}

	void ShaderCache::PrecompileVariantForMaterial(MaterialType type) {
		VertexLayout layout = VertexLayout::CreateLit();
		layout.Finalize();

		ShaderVariantKey key;
		key.baseName = MaterialTypeToShaderName(type);
		key.vertexLayoutHash = GenerateVertexLayoutHash(layout);
		key.actualLayout = layout;
		key.featureFlags = CombineFeatures(
			AnalyzeVertexLayout(layout),
			0,
			type
		);

		CreateShaderVariant(key);
	}

	// ========== CACHE MANAGEMENT ==========
	void ShaderCache::ClearCache() {
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		m_VariantCache.clear();
		m_Stats.totalVariants = 0;

		LogInfo("Shader cache cleared");
	}

	void ShaderCache::PruneUnusedVariants() {
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		if (m_VariantCache.size() <= m_Config.maxCachedVariants) {
			return; // No pruning needed
		}

		// Create vector of variants sorted by last usage
		std::vector<std::pair<ShaderVariantKey, size_t>> usageVector;
		for (const auto& [key, entry] : m_VariantCache) {
			usageVector.emplace_back(key, entry.lastUsedFrame);
		}

		// Sort by usage (oldest first)
		std::sort(usageVector.begin(), usageVector.end(),
			[](const auto& a, const auto& b) { return a.second < b.second; });

		// Remove least recently used variants
		size_t toRemove = m_VariantCache.size() - m_Config.maxCachedVariants;
		for (size_t i = 0; i < toRemove && i < usageVector.size(); ++i) {
			m_VariantCache.erase(usageVector[i].first);
			m_Stats.totalVariants--;
		}

		LogInfo("Pruned " + std::to_string(toRemove) + " shader variants from cache");
	}

	// ========== HOT RELOAD ==========
	void ShaderCache::EnableHotReload(bool enable) {
		m_Config.enableHotReload = enable;
		LogInfo("Hot reload " + std::string(enable ? "enabled" : "disabled"));
	}

	void ShaderCache::ReloadShader(const std::string& shaderPath) {
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		// Clear all variants that use this shader
		auto it = m_VariantCache.begin();
		while (it != m_VariantCache.end()) {
			if (it->second.vsPath == shaderPath || it->second.psPath == shaderPath) {
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

	void ShaderCache::ReloadAllShaders() {
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		size_t reloadedCount = m_VariantCache.size();
		m_VariantCache.clear();
		m_Stats.totalVariants = 0;
		m_Stats.hotReloads += reloadedCount;

		// Update all file timestamps
		for (const auto& [path, _] : m_FileTimestamps) {
			UpdateFileTimestamp(path);
		}

		LogInfo("Reloaded all shader variants (" + std::to_string(reloadedCount) + " variants)");
	}

	void ShaderCache::CheckForFileChanges() {
		std::vector<std::string> changedFiles;

		for (const auto& [filePath, storedTime] : m_FileTimestamps) {
			if (HasFileChanged(filePath)) {
				changedFiles.push_back(filePath);
			}
		}

		if (!changedFiles.empty()) {
			LogInfo("Hot reloading shaders due to file changes");
			for (const auto& file : changedFiles) {
				ReloadShader(file);
			}
		}
	}

	void ShaderCache::UpdateFileTimestamp(const std::string& filePath) {
		try {
			if (std::filesystem::exists(filePath)) {
				m_FileTimestamps[filePath] = GetFileModificationTime(filePath);
			}
		}
		catch (...) {
			// Ignore filesystem errors
		}
	}

	bool ShaderCache::HasFileChanged(const std::string& filePath) {
		try {
			if (!std::filesystem::exists(filePath)) {
				return false;
			}

			uint64_t currentTime = GetFileModificationTime(filePath);
			auto it = m_FileTimestamps.find(filePath);
			return it != m_FileTimestamps.end() && it->second != currentTime;
		}
		catch (...) {
			return false;
		}
	}
	std::pair<std::string, std::string> ShaderCache::GetShaderPaths(MaterialType materialType) {
		std::string basePath = m_Config.shaderDirectory;

		switch (materialType) {
		case MaterialType::Lit:
			return { basePath + "Lit.vs.hlsl", basePath + "Lit.ps.hlsl" };
		case MaterialType::PBR:
			return { basePath + "PBR.vs.hlsl", basePath + "PBR.ps.hlsl" };
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

	std::string ShaderCache::MaterialTypeToShaderName(MaterialType type) const {
		switch (type) {
		case MaterialType::Lit: return "Lit";
		case MaterialType::PBR: return "PBR";
		case MaterialType::Unlit: return "Unlit";
		case MaterialType::Transparent: return "Transparent";
		case MaterialType::Emissive: return "Emissive";
		case MaterialType::Skybox: return "Skybox";
		case MaterialType::UI: return "UI";
		default: return "Lit";
		}
	}

	std::string ShaderCache::LoadShaderSource(const std::string& path) {
		std::ifstream file(path);
		if (!file.is_open()) {
			return "";
		}

		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	std::string ShaderCache::GenerateVertexLayoutHash(const VertexLayout& layout) {
		std::string hash;
		for (const auto& attr : layout.GetAttributes()) {
			hash += std::to_string(static_cast<int>(attr.Type));
			hash += std::to_string(static_cast<int>(attr.Format));
			hash += std::to_string(attr.Slot);
			hash += "_";
		}
		return hash;
	}

	uint64_t ShaderCache::GetFileModificationTime(const std::string& path) {
		try {
			auto ftime = std::filesystem::last_write_time(path);
			return std::chrono::duration_cast<std::chrono::milliseconds>(
				ftime.time_since_epoch()).count();
		}
		catch (...) {
			return 0;
		}
	}

	std::string ShaderCache::GetDebugInfo() const {
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		std::ostringstream info;
		info << "=== Shader Cache Debug Info ===\n";
		info << "Initialized: " << (m_Initialized ? "Yes" : "No") << "\n";
		info << "Shader Directory: " << m_Config.shaderDirectory << "\n";
		info << "Hot Reload: " << (m_Config.enableHotReload ? "Enabled" : "Disabled") << "\n";
		info << "\n" << m_Stats.ToString();

		info << "\nBase Shaders (" << m_BaseShaders.size() << "):\n";
		for (const auto& [name, entry] : m_BaseShaders) {
			info << "  - " << name << "\n";
		}

		info << "\nCached Variants (" << m_VariantCache.size() << "):\n";
		for (const auto& [key, entry] : m_VariantCache) {
			info << "  - " << key.ToString() << " (Frame: " << entry.lastUsedFrame << ")\n";
		}

		return info.str();
	}

	void ShaderCache::LogError(const std::string& message) {
		OutputDebugStringA(("[ShaderCache ERROR] " + message + "\n").c_str());
	}

	void ShaderCache::LogWarning(const std::string& message) {
		OutputDebugStringA(("[ShaderCache WARNING] " + message + "\n").c_str());
	}

	void ShaderCache::LogInfo(const std::string& message) {
		OutputDebugStringA(("[ShaderCache INFO] " + message + "\n").c_str());
	}

	std::string ShaderCache::GenerateVertexLayoutHash(const VertexLayout& layout)
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

	namespace ShaderUtils {

		bool HasFeature(uint32_t flags, uint32_t feature) {
			return (flags & feature) != 0;
		}

		void SetFeature(uint32_t& flags, uint32_t feature, bool enabled) {
			if (enabled) {
				flags |= feature;
			}
			else {
				flags &= ~feature;
			}
		}

		std::string FeaturesToString(uint32_t flags) {
			std::vector<std::string> featureNames;

			if (HasFeature(flags, ShaderFeature::DiffuseMap)) featureNames.push_back("DiffuseMap");
			if (HasFeature(flags, ShaderFeature::NormalMap)) featureNames.push_back("NormalMap");
			if (HasFeature(flags, ShaderFeature::SpecularMap)) featureNames.push_back("SpecularMap");
			if (HasFeature(flags, ShaderFeature::EmissiveMap)) featureNames.push_back("EmissiveMap");
			if (HasFeature(flags, ShaderFeature::EnvironmentMap)) featureNames.push_back("EnvMap");
			if (HasFeature(flags, ShaderFeature::RoughnessMap)) featureNames.push_back("RoughnessMap");
			if (HasFeature(flags, ShaderFeature::MetallicMap)) featureNames.push_back("MetallicMap");
			if (HasFeature(flags, ShaderFeature::AOMap)) featureNames.push_back("AOMap");
			if (HasFeature(flags, ShaderFeature::HeightMap)) featureNames.push_back("HeightMap");

			if (HasFeature(flags, ShaderFeature::HasTexCoords)) featureNames.push_back("TexCoords");
			if (HasFeature(flags, ShaderFeature::HasNormals)) featureNames.push_back("Normals");
			if (HasFeature(flags, ShaderFeature::HasTangents)) featureNames.push_back("Tangents");
			if (HasFeature(flags, ShaderFeature::HasVertexColors)) featureNames.push_back("VertexColors");
			if (HasFeature(flags, ShaderFeature::HasSkinning)) featureNames.push_back("Skinning");
			if (HasFeature(flags, ShaderFeature::HasSecondTexCoords)) featureNames.push_back("SecondUV");

			if (HasFeature(flags, ShaderFeature::Shadows)) featureNames.push_back("Shadows");
			if (HasFeature(flags, ShaderFeature::Fog)) featureNames.push_back("Fog");
			if (HasFeature(flags, ShaderFeature::Instancing)) featureNames.push_back("Instancing");
			if (HasFeature(flags, ShaderFeature::AlphaTest)) featureNames.push_back("AlphaTest");
			if (HasFeature(flags, ShaderFeature::ParallaxMapping)) featureNames.push_back("Parallax");

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

		MaterialType DeduceMaterialType(const Material* material) {
			if (!material) return MaterialType::Lit;
			return material->GetType();
		}

		uint32_t ExtractMaterialFeatures(const Material* material) {
			if (!material) return 0;

			uint32_t features = 0;
			uint32_t matFlags = material->GetFeatureFlags();

			if (matFlags & (1 << 0)) features |= ShaderFeature::DiffuseMap;
			if (matFlags & (1 << 1)) features |= ShaderFeature::NormalMap;
			if (matFlags & (1 << 2)) features |= ShaderFeature::SpecularMap;
			if (matFlags & (1 << 3)) features |= ShaderFeature::EmissiveMap;
			if (matFlags & (1 << 4)) features |= ShaderFeature::EnvironmentMap;
			if (matFlags & (1 << 5)) features |= ShaderFeature::RoughnessMap;
			if (matFlags & (1 << 6)) features |= ShaderFeature::MetallicMap;
			if (matFlags & (1 << 7)) features |= ShaderFeature::AOMap;

			return features;
		}
	}



}