#include "dxpch.h"
#include "Material.h"
#include "renderer/ShaderCache.h"
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

	void Material::SetTexture(TextureSlot slot, std::shared_ptr < RHI::ITexture> texture)
	{
		int textureSlot = static_cast<int>(slot);
		if (textureSlot < m_Textures.size())
		{
			m_Textures[textureSlot] = texture;

			// Keep the shader-feature flags in sync with what is bound. Setting nullptr must also
			// switch the feature OFF, otherwise the shader variant still expects a texture that is gone.
			// Switch on the enum, not on raw integers: the old numeric cases were shifted by one from
			// slot 4 on (a roughness texture enabled EnvironmentMap), so materials picked the wrong
			// shader variant. The names match the register comments in common.hlsli.
			uint32_t feature = 0;
			switch (slot) {
			case TextureSlot::Diffuse:          feature = ShaderFeature::DiffuseMap;       break;
			case TextureSlot::Normal:           feature = ShaderFeature::NormalMap;        break;
			case TextureSlot::Specular:         feature = ShaderFeature::SpecularMap;      break;
			case TextureSlot::Emissive:         feature = ShaderFeature::EmissiveMap;      break;
			case TextureSlot::Roughness:        feature = ShaderFeature::RoughnessMap;     break;
			case TextureSlot::Metallic:         feature = ShaderFeature::MetallicMap;      break;
			case TextureSlot::AmbientOcclusion: feature = ShaderFeature::AOMap;            break;
			case TextureSlot::Height:           feature = ShaderFeature::HeightMap;        break;
			case TextureSlot::Opacity:          feature = ShaderFeature::OpacityMap;       break;
			case TextureSlot::DetailDiffuse:    feature = ShaderFeature::DetailDiffuseMap; break;
			case TextureSlot::DetailNormal:     feature = ShaderFeature::DetailNormalMap;  break;
			case TextureSlot::Environment:      feature = ShaderFeature::EnvironmentMap;   break; // TODO: irradiance + BRDF LUT features for IBL
			default: break; // no feature for this slot
			}
			if (feature != 0)
			{
				if (texture) EnableFeature(feature); else DisableFeature(feature);
			}

			// the combined "details" switch is on while at least one detail texture is bound
			if (slot == TextureSlot::DetailDiffuse || slot == TextureSlot::DetailNormal)
			{
				const bool anyDetail = m_Textures[static_cast<int>(TextureSlot::DetailDiffuse)] ||
				                       m_Textures[static_cast<int>(TextureSlot::DetailNormal)];
				if (anyDetail) EnableFeature(ShaderFeature::UseDetailsTextures);
				else           DisableFeature(ShaderFeature::UseDetailsTextures);
			}
		}
		m_Dirty = true;
	}
	std::shared_ptr<RHI::ITexture> Material::GetTexture(TextureSlot slot)
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

		if (m_ConstantBuffer)
		{
			m_ConstantBuffer->Update(&m_Properties, sizeof(MaterialProperties));
			m_Dirty = false;
		}
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

		if (roughness > 0.1f && m_Type == MaterialType::Lit)
		{
			m_Type = MaterialType::PBR;
		}
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
		auto mat = std::make_shared<Material>(name, MaterialType::Unlit);
		mat->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateLitMaterial(const std::string& name)
	{
		auto mat = std::make_shared<Material>(name, MaterialType::Lit);
		mat->SetDiffuseColor({ 0.8f, 0.8f, 0.8f, 1.0f });
		mat->SetSpecularColor({ 0.5f, 0.5f, 0.5f, 1.0f });
		mat->SetShininess(32.0f);
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreatePBRMaterial(const std::string& name)
	{
		auto mat = std::make_shared<Material>(name, MaterialType::PBR);
		mat->SetDiffuseColor({ 0.8f, 0.8f, 0.8f, 1.0f });
		mat->SetMetallic(0.0f);
		mat->SetRoughness(0.5f);
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateEmissiveMaterial(const std::string& name)
	{
		auto mat = std::make_shared<Material>(name, MaterialType::Emissive);
		mat->SetEmissiveColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		mat->SetEmissiveIntensity(3.0f);
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateSkyboxMaterial(const std::string& name)
	{
		auto mat = std::make_shared<Material>(name, MaterialType::Skybox);
		mat->SetCullMode(RHI::CullMode::Front);
		mat->SetDepthTest(RHI::DepthTestMode::LessEqual);
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateTransparentMaterial(const std::string& name)
	{
		auto mat = std::make_shared<Material>(name, MaterialType::Transparent);
		mat->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 0.5f });
		mat->SetBlendMode(RHI::BlendMode::AlphaBlend);
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateUIMaterial(const std::string& name)
	{
		auto mat = std::make_shared<Material>(name, MaterialType::UI);
		mat->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		mat->SetDepthTest(RHI::DepthTestMode::None);
		mat->SetBlendMode(RHI::BlendMode::AlphaBlend);
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateFromConfig(const std::string& configPath)
	{
		// TODO: Implement material loading from file (JSON, XML, etc.)
		return CreateLitMaterial("DefaultFromConfig");
	}

}