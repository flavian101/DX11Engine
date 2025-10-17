#include "dxpch.h"
#include "Material.h"
#include "utils/Texture.h"
#include "utils/CubeMapTexture.h"
#include "shaders/ShaderManager.h"
#include <algorithm>

namespace DXEngine {

	uint32_t Material::s_NextID = 1;

	Material::Material(const std::string& name, MaterialType type)
		: m_Name(name)
		, m_ID(s_NextID++)
		, m_Type(type)
		, m_Textures(static_cast<size_t>(TextureSlot::MaxTextureSlots), nullptr)
	{}

	Material::~Material()
	{}

	void Material::SetTexture(TextureSlot slot, RHI::ITexture* texture)
	{
		int textureSlot = static_cast<int>(slot);
		if (textureSlot < m_Textures.size())
		{
			m_Textures[textureSlot] = texture;

			//auto enable feature flags based on texture slot
			if (texture) {
				switch (textureSlot) {
				case 0: EnableFeature(ShaderFeature::DiffuseMap);        break;
				case 1: EnableFeature(ShaderFeature::NormalMap);         break;
				case 2: EnableFeature(ShaderFeature::SpecularMap);       break;
				case 3: EnableFeature(ShaderFeature::EmissiveMap);       break;
				case 4: EnableFeature(ShaderFeature::EnvironmentMap);    break; //TO-DO add irradiance and BRDF_LUT features for IBL
				case 5: EnableFeature(ShaderFeature::RoughnessMap);      break;
				case 6: EnableFeature(ShaderFeature::MetallicMap);       break;
				case 7: EnableFeature(ShaderFeature::AOMap);             break;
				case 8: EnableFeature(ShaderFeature::HeightMap);         break;
				case 9: EnableFeature(ShaderFeature::OpacityMap);        break;
				case 10:
					EnableFeature(ShaderFeature::DetailDiffuseMap);
					EnableFeature(ShaderFeature::UseDetailsTextures);
					break;
				case 11:
					EnableFeature(ShaderFeature::DetailNormalMap);
					EnableFeature(ShaderFeature::UseDetailsTextures);
					break;
				default: break; // no feature for this slot
				}
			}
		}
	}
	RHI::ITexture* Material::GetTexture(TextureSlot slot)
	{
		int textureSlot = static_cast<int>(slot);
		return  textureSlot < m_Textures.size() ? m_Textures[textureSlot] : nullptr;
	}

	void Material::UpdateConstantBuffer(RHI::IGraphicsDevice* device)
	{
		if (!m_Dirty) return;

		if (!m_ConstantBuffer)
		{
			RHI::BufferDesc desc;
			desc.type = RHI::BufferType::Uniform;
			desc.usage = RHI::BufferUsage::Dynamic;
			desc.size = sizeof(MaterialProperties);
			desc.debugName = "Material_" + m_Name;
			m_ConstantBuffer = device->CreateBuffer(desc);
		}

		m_ConstantBuffer->Update(&m_Properties, sizeof(MaterialProperties));
		m_Dirty = false;
	}
	std::string Material::GetShaderName() const {
		// Shader name is derived from material type
		switch (m_Type) {
		case MaterialType::Unlit: return "Unlit";
		case MaterialType::Lit: return "Lit";
		case MaterialType::PBR: return "PBR";
		case MaterialType::Transparent: return "Transparent";
		case MaterialType::UI: return "UI";
		case MaterialType::Skybox: return "Skybox";
		default: return "Basic";
		}
	}

	void Material::SetDiffuseColor(const DirectX::XMFLOAT4& color)
	{
		m_Properties.diffuseColor = color;
		m_Dirty = true;
	}

	void Material::SetSpecularColor(const DirectX::XMFLOAT4& color)
	{
		m_Properties.specularColor = color;
		m_Dirty = true;
	}

	void Material::SetEmissiveColor(const DirectX::XMFLOAT4& color)
	{
		m_Properties.emissiveColor = color;
		m_Dirty = true;
	}

	void Material::SetShininess(float shininess)
	{
		m_Properties.shininess = std::max(1.0f, shininess);
		m_Dirty = true;
	}

	void Material::SetAlpha(float alpha)
	{
		m_Properties.alpha = std::clamp(alpha, 0.0f, 1.0f);
		m_Dirty = true;
	}

	void Material::SetMetallic(float metallic)
	{
		m_Properties.metallic = std::clamp(metallic, 0.0f, 1.0f);
		m_Dirty = true;

		if (metallic > 0.1f && m_Type == MaterialType::Lit)
		{
			m_Type = MaterialType::PBR;
		}
	}

	void Material::SetRoughness(float roughness)
	{
		m_Properties.roughness = std::clamp(roughness, 0.04f, 1.0f);
		m_Dirty = true;
	}

	void Material::SetNormalScale(float scale)
	{
		m_Properties.normalScale = std::max(0.0f, scale);
		m_Dirty = true;
	}

	void Material::SetHeightScale(float scale)
	{
		m_Properties.heightScale = std::clamp(scale, 0.0f, 0.2f);
		m_Dirty = true;
	}

	void Material::SetOcculsionStrength(float strength)
	{
		m_Properties.occlusionStrength = std::clamp(strength, 0.0f, 1.0f);
		m_Dirty = true;
	}

	void Material::SetEmissiveIntensity(float intensity)
	{
		m_Properties.emissiveIntensity = std::max(0.0f, intensity);
		m_Dirty = true;
	}
	void Material::SetDetailTextureScale(const DirectX::XMFLOAT2& scale)
	{
		m_Properties.detailScale = scale;
		m_Dirty = true;
	}
	void Material::SetDetailTextureOffset(const DirectX::XMFLOAT2& offset)
	{
		m_Properties.detailOffset = offset;
		m_Dirty = true;
	}

	void Material::SetTextureScale(const DirectX::XMFLOAT2& scale)
	{
		m_Properties.textureScale = scale;
		m_Dirty = true;
	}

	void Material::SetTextureOffset(const DirectX::XMFLOAT2& offset)
	{
		m_Properties.textureOffset = offset;
		m_Dirty = true;
	}
	
	std::shared_ptr<Material> MaterialFactory::CreateUnlitMaterial(const std::string& name)
	{
		return std::make_shared<Material>(name, MaterialType::Unlit);
	}

	std::shared_ptr<Material> MaterialFactory::CreateLitMaterial(const std::string& name)
	{
		return std::make_shared<Material>(name, MaterialType::Lit);
	}

	std::shared_ptr<Material> MaterialFactory::CreatePBRMaterial(const std::string& name)
	{
		return std::make_shared<Material>(name, MaterialType::PBR);
	}

	std::shared_ptr<Material> MaterialFactory::CreateEmissiveMaterial(const std::string& name)
	{
		return std::make_shared<Material>(name, MaterialType::Emissive);
	}

	std::shared_ptr<Material> MaterialFactory::CreateSkyboxMaterial(const std::string& name)
	{
		return std::make_shared<Material>(name, MaterialType::Skybox);
	}

	std::shared_ptr<Material> MaterialFactory::CreateTransparentMaterial(const std::string& name)
	{
		return std::make_shared<Material>(name, MaterialType::Transparent);
	}

	std::shared_ptr<Material> MaterialFactory::CreateUIMaterial(const std::string& name)
	{
		return std::make_shared<Material>(name, MaterialType::UI);
	}

	std::shared_ptr<Material> MaterialFactory::CreateFromConfig(const std::string& configPath)
	{
		// TODO: Implement material loading from file (JSON, XML, etc.)
		return CreateLitMaterial("DefaultFromConfig");
	}

}