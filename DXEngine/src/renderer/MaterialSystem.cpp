#include "dxpch.h"
#include "MaterialSystem.h"
#include "renderer/ShaderCache.h"

namespace DXEngine::Rendering
{

	MaterialSystem::MaterialSystem(std::shared_ptr<RHI::IGraphicsDevice> device, std::shared_ptr<ShaderCache> shaderCache)
		:m_Device(device),
		m_ShaderCache(shaderCache)
	{
		if (!m_Device || !m_ShaderCache) {
			throw std::runtime_error("MaterialSystem: Device and ShaderCache cannot be null");
		}
	}
	MaterialSystem::~MaterialSystem()
	{
		Shutdown();
	}
	bool MaterialSystem::Initialize()
	{
		OutputDebugStringA("MaterialSystem: Initializing...\n");

		CreateDefaultMaterials();

		OutputDebugStringA("MaterialSystem: Initialized\n");
		return true;
	}
	void MaterialSystem::Shutdown()
	{
		m_Materials.clear();
		m_DefaultMaterials.clear();
		OutputDebugStringA("MaterialSystem: Shutdown complete\n");

	}
	std::shared_ptr<Material> MaterialSystem::CreateMaterial(const std::string& name, MaterialType type)
	{
		auto it = m_Materials.find(name);
		if (it != m_Materials.end()) {
			OutputDebugStringA(("MaterialSystem: Material already exists: " +
				name + "\n").c_str());
			return it->second;
		}

		auto material = std::make_shared<Material>(name, type);
		m_Materials[name] = material;

		OutputDebugStringA(("MaterialSystem: Created material: " + name + "\n").c_str());
		return material;
	}
	std::shared_ptr<Material> MaterialSystem::GetMaterial(const std::string& name)
	{
		auto it = m_Materials.find(name);
		return it != m_Materials.end() ? it->second : nullptr;
	}
	std::shared_ptr<Material> MaterialSystem::GetDefaultMaterial(MaterialType type)
	{
		auto it = m_DefaultMaterials.find(type);
		return it != m_DefaultMaterials.end() ? it->second : nullptr;
	}
	void MaterialSystem::UpdateMaterials()
	{
		for (auto& [name, material] : m_Materials) {
			material->UpdateConstantBuffer(m_Device.get());
		}
	}
	void MaterialSystem::RemoveMaterial(const std::string& name)
	{
		m_Materials.erase(name);
	}
	std::shared_ptr<ShaderProgram> MaterialSystem::GetShaderForMaterial(Material* material, const VertexLayout& layout)
	{
		if (!material) return nullptr;

		// Create shader variant key based on material properties
		ShaderVariantKey key = CreateVariantKey(material, layout);

		// Get shader variant from cache
		return m_ShaderCache->GetShaderVariant(key);
	}

	ShaderVariantKey MaterialSystem::CreateVariantKey(Material* material, const VertexLayout& layout)
	{
		ShaderVariantKey key;
		key.baseName = material->GetShaderName();
		key.featureFlags = material->GetFeatureFlags();
		key.actualLayout = layout;

		// Add lighting features based on material type
		if (material->GetType() == MaterialType::Lit ||
			material->GetType() == MaterialType::PBR) {
			key.featureFlags |= ShaderFeature::Lighting;
		}

		return key;
	}
	void MaterialSystem::CreateDefaultMaterials() {
		// Unlit default
		auto unlit = CreateMaterial("DefaultUnlit", MaterialType::Unlit);
		unlit->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		m_DefaultMaterials[MaterialType::Unlit] = unlit;

		// Lit default
		auto lit = CreateMaterial("DefaultLit", MaterialType::Lit);
		lit->SetDiffuseColor({ 0.8f, 0.8f, 0.8f, 1.0f });
		lit->EnableFeature(ShaderFeature::Lighting);
		m_DefaultMaterials[MaterialType::Lit] = lit;

		// PBR default
		auto pbr = CreateMaterial("DefaultPBR", MaterialType::PBR);
		pbr->SetDiffuseColor({ 0.8f, 0.8f, 0.8f, 1.0f });
		pbr->SetMetallic(0.0f);
		pbr->SetRoughness(0.5f);
		pbr->EnableFeature(ShaderFeature::Lighting);
		m_DefaultMaterials[MaterialType::PBR] = pbr;

		// Transparent default
		auto transparent = CreateMaterial("DefaultTransparent", MaterialType::Transparent);
		transparent->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 0.5f });
		transparent->SetBlendMode(RHI::BlendMode::AlphaBlend);
		m_DefaultMaterials[MaterialType::Transparent] = transparent;

		// UI default
		auto ui = CreateMaterial("DefaultUI", MaterialType::UI);
		ui->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		ui->SetCullMode(RHI::CullMode::None);
		ui->SetDepthTest(RHI::DepthTestMode::None);
		m_DefaultMaterials[MaterialType::UI] = ui;

		OutputDebugStringA("MaterialSystem: Default materials created\n");
	}

	size_t MaterialSystem::GetMaterialCount() const
	{
		return m_Materials.size();
	}

}