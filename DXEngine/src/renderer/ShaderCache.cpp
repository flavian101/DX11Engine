#include "dxpch.h"
#include "ShaderCache.h"
#include <filesystem>
#include <fstream>


namespace DXEngine
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

		// Load the base shader of every shading model (the file names come from the ShadingModels table)
		for (const ShadingModelInfo& info : ShadingModels::All())
			LoadBaseShader(info.name, info.vertexShader, info.pixelShader);

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
		m_ShadowCasterPrograms[0].reset();
		m_ShadowCasterPrograms[1].reset();
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
			return GetFallbackShader(ShadingModel::Lit);
		}

		if (!material) {
			LogWarning("Material is null, using fallback");
			return GetFallbackShader(ShadingModel::Lit);
		}

		// Build variant key
		ShaderVariantKey key;
		key.baseName = ShadingModels::Name(material->GetShadingModel());
		key.vertexLayoutHash = GenerateVertexLayoutHash(layout);
		key.actualLayout = layout; //Store the actual layout for compilation

		// Analyze features
		key.featureFlags = CombineFeatures(AnalyzeVertexLayout(layout), material->GetShadingModel(), material->IsBlended());
		key.textureMask = material->GetTextureMask();      // which textures it has: decided by the slot table

		// Try to get cached variant
		auto shader = GetShaderVariant(key);
		if (!shader) {
			LogWarning("Failed to get shader variant, using fallback");
			return GetFallbackShader(material->GetShadingModel());
		}

		return shader;
	}

	std::shared_ptr<ShaderProgram> ShaderCache::GetShadowCasterShader(const VertexLayout& layout)
	{
		const bool skinned = layout.HasAttribute(RHI::VertexAttributeType::BlendWeights);
		const size_t index = skinned ? 1 : 0;

		std::lock_guard<std::mutex> lock(m_CacheMutex);
		if (m_ShadowCasterPrograms[index])
			return m_ShadowCasterPrograms[index];

		const std::string path = m_Config.shaderDirectory + "ShadowCaster.vs.hlsl";
		const std::string defines = skinned ? "#define HAS_SKINNING_ATTRIBUTES 1\n" : "";

		std::shared_ptr<RHI::IShader> vs;
		try
		{
			vs = CompileShaderFromFile(path, RHI::ShaderStage::Vertex, defines);
		}
		catch (const std::exception& e)
		{
			LogError("Shadow caster shader failed to compile: " + std::string(e.what()));
		}
		if (!vs)
			return nullptr;

		auto program = ShaderProgram::CreateDepthOnly(vs);
		if (!program || !program->IsValid())
			return nullptr;

		m_ShadowCasterPrograms[index] = program;   // (cleared with the rest in ClearCache / Shutdown)
		return program;
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

			ShadingModel model = ShadingModel::Lit;
			ShadingModels::Parse(key.baseName, model);   // the key was built from ShadingModels::Name, so this cannot miss

			auto [vsPath, psPath] = GetShaderPaths(model);

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

	std::shared_ptr<ShaderProgram>ShaderCache::GetFallbackShader(ShadingModel model)
	{
		// Check if we have a fallback for this material type
		auto it = m_FallbackShaders.find(model);
		if (it != m_FallbackShaders.end()) {
			return it->second;
		}

		// Create minimal fallback shader
		VertexLayout fallbackLayout;
		fallbackLayout.Position();

		switch (model) {
		case ShadingModel::UI:
			fallbackLayout.TexCoord(0);
			fallbackLayout.Color(0);
			break;
		case ShadingModel::Sky:
			fallbackLayout.Normal();
			break;
		default:
			fallbackLayout.Normal();
			fallbackLayout.TexCoord(0);
			break;
		}

		fallbackLayout.Finalize();

		ShaderVariantKey key;
		key.baseName = ShadingModels::Name(model);
		key.vertexLayoutHash = GenerateVertexLayoutHash(fallbackLayout);
		key.featureFlags = AnalyzeVertexLayout(fallbackLayout);
		key.actualLayout = fallbackLayout;
		auto fallback = CreateShaderVariant(key);
		if (fallback) {
			m_FallbackShaders[model] = fallback;
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

		if (!vsShader || !psShader)
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
		ShadingModel model = ShadingModel::Lit;
		if (!ShadingModels::Parse(key.baseName, model))
		{
			LogError("Unknown shading model in shader key: " + key.baseName);   // (it used to silently become UI)
			return nullptr;
		}
		auto [vsPath, psPath] = GetShaderPaths(model);

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
		std::string defines = GenerateDefinesString(key.featureFlags, key.textureMask, key.actualLayout);

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

		// Prepend: the texture declarations (generated from the slot table, so no shader declares a texture register by
		// hand), then the #defines. "#line 1" makes compiler messages count from the shader file's own first line again
		// instead of from the top of this generated text.
		std::string finalSource = MaterialSlots::GenerateHlslDeclarations() + defines + "\n#line 1\n" + source;

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

	uint32_t ShaderCache::CombineFeatures(uint32_t layoutFeatures, ShadingModel model, bool blended)
	{
		uint32_t combined = layoutFeatures;

		// A model lit by the scene can RECEIVE shadows. It is a compile-time feature on purpose (one variant, not two): a light
		// with no shadow, or an object with receiveShadows off, skips the lookup at run time.
		if (ShadingModels::Info(model).litByScene)
			combined |= ShaderFeature::Lighting | ShaderFeature::Shadows;

		// Blending is render STATE, not a shading model: any model can be blended, and then only needs to drop the pixels that
		// would be invisible anyway.
		if (blended)
			combined |= ShaderFeature::Blended;

		return combined;
	}

	// ========== DEFINE STRING GENERATION ==========
	std::string ShaderCache::GenerateDefinesString(uint32_t features, uint32_t textureMask, const VertexLayout& layout) {
		std::ostringstream defines;

		// Texture features (HAS_DIFFUSE_TEXTURE, HAS_NORMAL_MAP, ... plus HAS_DETAIL_TEXTURES and ENABLE_PARALLAX_MAPPING):
		// one function reads the slot table. This used to be thirteen hand-written if-lines.
		defines << MaterialSlots::GenerateTextureDefines(textureMask);

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
		if ((features & ShaderFeature::AlphaTest) || (features & ShaderFeature::Blended))
			defines << "#define ENABLE_ALPHA_TEST 1\n";
		if (features & ShaderFeature::Blended)
			defines << "#define ALPHA_TEST_THRESHOLD 0.1\n";      // (cut-out materials keep surface.hlsli's 0.5)

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
		// (a blended material is the same model with the Blended feature: the common blended Lit variant is added below)
		std::vector<ShadingModel> models = {
			ShadingModel::Lit,
			ShadingModel::Unlit,
			ShadingModel::UI
		};

		size_t variantsCompiled = 0;

		for (auto& layout : layouts) {
			layout.Finalize();

			for (ShadingModel model : models) {
				// Skip UI shaders for non-UI layouts
				if (model == ShadingModel::UI &&
					!layout.HasAttribute(RHI::VertexAttributeType::Color0)) {
					continue;
				}

				ShaderVariantKey key;
				key.baseName = ShadingModels::Name(model);
				key.vertexLayoutHash = GenerateVertexLayoutHash(layout);
				key.actualLayout = layout;
				key.featureFlags = CombineFeatures(AnalyzeVertexLayout(layout), model, false);   // no textures: mask 0

				if (CreateShaderVariant(key)) {
					variantsCompiled++;
				}
				if (model == ShadingModel::Lit) {          // the blended twin (glass, fades)
					key.featureFlags = CombineFeatures(AnalyzeVertexLayout(layout), model, true);
					if (CreateShaderVariant(key)) variantsCompiled++;
				}
			}
		}

		LogInfo("Precompiled " + std::to_string(variantsCompiled) + " shader variants");
	}

	void ShaderCache::PrecompileVariantForMaterial(ShadingModel model) {
		VertexLayout layout = VertexLayout::CreateLit();
		layout.Finalize();

		ShaderVariantKey key;
		key.baseName = ShadingModels::Name(model);
		key.vertexLayoutHash = GenerateVertexLayoutHash(layout);
		key.actualLayout = layout;
		key.featureFlags = CombineFeatures(AnalyzeVertexLayout(layout), model, false);

		CreateShaderVariant(key);
	}

	// ========== CACHE MANAGEMENT ==========
	void ShaderCache::ClearCache() {
		std::lock_guard<std::mutex> lock(m_CacheMutex);

		m_VariantCache.clear();
		m_ShadowCasterPrograms[0].reset();
		m_ShadowCasterPrograms[1].reset();
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
	std::pair<std::string, std::string> ShaderCache::GetShaderPaths(ShadingModel model) {
		// the file names live in ONE table (ShadingModels in MaterialTypes.cpp)
		const ShadingModelInfo& info = ShadingModels::Info(model);
		return { m_Config.shaderDirectory + info.vertexShader, m_Config.shaderDirectory + info.pixelShader };
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
			if (HasFeature(flags, ShaderFeature::Blended)) featureNames.push_back("Blended");

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
	}



}