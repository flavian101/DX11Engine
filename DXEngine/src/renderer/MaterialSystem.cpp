#include "dxpch.h"
#include "MaterialSystem.h"
#include "renderer/ShaderCache.h"

namespace DXEngine
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
	std::shared_ptr<Material> MaterialSystem::CreateMaterial(const std::string& name, ShadingModel model)
	{
		auto it = m_Materials.find(name);
		if (it != m_Materials.end()) {
			OutputDebugStringA(("MaterialSystem: Material already exists: " +
				name + "\n").c_str());
			return it->second;
		}

		auto material = std::make_shared<Material>(name, model);
		m_Materials[name] = material;

		OutputDebugStringA(("MaterialSystem: Created material: " + name + "\n").c_str());
		return material;
	}
	std::shared_ptr<Material> MaterialSystem::GetMaterial(const std::string& name)
	{
		auto it = m_Materials.find(name);
		return it != m_Materials.end() ? it->second : nullptr;
	}
	std::shared_ptr<Material> MaterialSystem::GetDefaultMaterial(ShadingModel model)
	{
		auto it = m_DefaultMaterials.find(model);
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
	void MaterialSystem::CreateDefaultMaterials() {
		// One default per shading model that can be used on its own. (There is no "default PBR" or "default transparent":
		// they are the Lit model; a transparent material is a Lit material with a blended RenderState.)
		auto add = [this](const std::string& name, std::shared_ptr<Material> material)
		{
			material->SetName(name);
			m_Materials[name] = material;
			m_DefaultMaterials[material->GetShadingModel()] = material;
		};

		add("DefaultUnlit",    MaterialFactory::CreateUnlitMaterial());
		add("DefaultLit",      MaterialFactory::CreateLitMaterial());
		add("DefaultEmissive", MaterialFactory::CreateEmissiveMaterial());
		add("DefaultUI",       MaterialFactory::CreateUIMaterial());

		OutputDebugStringA("MaterialSystem: Default materials created\n");
	}

	size_t MaterialSystem::GetMaterialCount() const
	{
		return m_Materials.size();
	}

}