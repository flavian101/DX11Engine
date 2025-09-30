#include "dxpch.h"
#include "Material.h"
#include "utils/Texture.h"
#include "utils/CubeMapTexture.h"
#include "shaders/ShaderManager.h"
#include "renderer/RendererCommand.h"
#include <algorithm>

namespace DXEngine {

	Material::Material(const std::string& name, MaterialType type)
		: m_Name(name),m_Type(type),m_RenderQueue(RenderQueue::Opaque)
	{
		switch (type)
		{
		case MaterialType::Skybox:
			m_RenderQueue = RenderQueue::Background;
			break;
		case MaterialType::Transparent:
			m_RenderQueue = RenderQueue::Transparent;
			break;
		case MaterialType::UI:
			m_RenderQueue = RenderQueue::UI;
			break;
		default:
			m_RenderQueue = RenderQueue::Opaque;
			break;
		}

		// Initialize default properties based on type
		switch (type)
		{
		case MaterialType::Unlit:
			m_Properties.diffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			SetFlag(MaterialFlags::ReceivesShadows, false);
			break;

		case MaterialType::Skybox:
			m_Properties.diffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			SetFlag(MaterialFlags::CastsShadows, false);
			SetFlag(MaterialFlags::ReceivesShadows, false);
			SetFlag(MaterialFlags::IsTwoSided, true);
			break;

		case MaterialType::Transparent:
			m_Properties.alpha = 0.5f;
			SetFlag(MaterialFlags::IsTransparent, true);
			break;
		case MaterialType::UI:
			m_Properties.alpha = 0.5f;
			SetFlag(MaterialFlags::IsTransparent, true);
			SetFlag(MaterialFlags::IsTransparent, true);
			break;

		default:
			// Default lit material
			m_Properties.diffuseColor = { 0.8f, 0.8f, 0.8f, 1.0f };
			m_Properties.specularColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			m_Properties.shininess = 32.0f;
			break;
		}

		UpdateTextureFlags();
	}

	Material::~Material()
	{
	}
	void Material::Bind()
	{
		if (!IsValid()) return;

		if (m_PropertiesDirty)
		{
			UpdateConstantBuffer();
			m_PropertiesDirty = false;
		}

		if (m_ConstantBufferInitialized)
		{
			RenderCommand::GetContext()->PSSetConstantBuffers(BindSlot::CB_Material, 1, m_ConstantBuffer.GetAddressOf());
		}

		// Bind all available textures
		if (m_Resources.diffuseTexture)
			m_Resources.diffuseTexture->Bind(static_cast<UINT>(TextureSlot::Diffuse));
		if (m_Resources.normalTexture)
			m_Resources.normalTexture->Bind(static_cast<UINT>(TextureSlot::Normal));
		if (m_Resources.specularTexture)
			m_Resources.specularTexture->Bind(static_cast<UINT>(TextureSlot::Specular));
		if (m_Resources.emissiveTexture)
			m_Resources.emissiveTexture->Bind(static_cast<UINT>(TextureSlot::Emissive));
		if (m_Resources.roughnessTexture)
			m_Resources.roughnessTexture->Bind(static_cast<UINT>(TextureSlot::Roughness));
		if (m_Resources.metallicTexture)
			m_Resources.metallicTexture->Bind(static_cast<UINT>(TextureSlot::Metallic));
		if (m_Resources.aoTexture)
			m_Resources.aoTexture->Bind(static_cast<UINT>(TextureSlot::AmbientOcclusion));
		if (m_Resources.heightTexture)
			m_Resources.heightTexture->Bind(static_cast<UINT>(TextureSlot::Height),true,true);
		if (m_Resources.opacityTexture)
			m_Resources.opacityTexture->Bind(static_cast<UINT>(TextureSlot::Opacity));
		if (m_Resources.detailDiffuseTexture)
			m_Resources.detailDiffuseTexture->Bind(static_cast<UINT>(TextureSlot::DetailDiffuse));
		if (m_Resources.detailNormalTexture)
			m_Resources.detailNormalTexture->Bind(static_cast<UINT>(TextureSlot::DetailNormal));
		if (m_Resources.environmentTexture)
			m_Resources.environmentTexture->Bind(static_cast<UINT>(TextureSlot::Environment));

	}
	bool Material::IsValid() const
	{
		return m_Resources.IsValid();
	}
	void Material::SetType(MaterialType type)
	{
		if (m_Type != type)
		{
			m_Type = type;
			m_PropertiesDirty = true;

			switch (type)
			{
			case MaterialType::Skybox:
				m_RenderQueue = RenderQueue::Background;
				break;
			case MaterialType::Transparent:
				m_RenderQueue = RenderQueue::Transparent;
				break;
			default:
				if (m_RenderQueue == RenderQueue::Background || m_RenderQueue == RenderQueue::Transparent)
				{
					m_RenderQueue = RenderQueue::Opaque;
				}
				break;
			}

		}
	}

	void Material::SetDiffuseColor(const DirectX::XMFLOAT4& color)
	{
		m_Properties.diffuseColor = color;
		m_PropertiesDirty = true;
	}

	void Material::SetSpecularColor(const DirectX::XMFLOAT4& color)
	{
		m_Properties.specularColor = color;
		m_PropertiesDirty = true;
	}

	void Material::SetEmissiveColor(const DirectX::XMFLOAT4& color)
	{
		m_Properties.emissiveColor = color;
		m_PropertiesDirty = true;
	}

	void Material::SetShininess(float shininess)
	{
		m_Properties.shininess = std::max(1.0f, shininess);
		m_PropertiesDirty = true;
	}

	void Material::SetAlpha(float alpha)
	{
		m_Properties.alpha = std::clamp(alpha, 0.0f, 1.0f);
		m_PropertiesDirty = true;

		// Update transparency flag
		SetFlag(MaterialFlags::IsTransparent, alpha < 1.0f);
	}

	void Material::SetMetallic(float metallic)
	{
		m_Properties.metallic = std::clamp(metallic, 0.0f, 1.0f);
		m_PropertiesDirty = true;

		if (metallic > 0.1f && m_Type == MaterialType::Lit)
		{
			m_Type = MaterialType::PBR;
		}
	}

	void Material::SetRoughness(float roughness)
	{
		m_Properties.roughness = std::clamp(roughness, 0.04f, 1.0f);
		m_PropertiesDirty = true;
	}

	void Material::SetNormalScale(float scale)
	{
		m_Properties.normalScale = std::max(0.0f, scale);
		m_PropertiesDirty = true;
	}

	void Material::SetHeightScale(float scale)
	{
		m_Properties.heightScale = std::clamp(scale, 0.0f, 0.2f);
		m_PropertiesDirty = true;
	}

	void Material::SetOcculsionStrength(float strength)
	{
		m_Properties.occlusionStrength = std::clamp(strength, 0.0f, 1.0f);
		m_PropertiesDirty = true;
	}

	void Material::SetEmissiveIntensity(float intensity)
	{
		m_Properties.emissiveIntensity = std::max(0.0f, intensity);
		m_PropertiesDirty = true;
	}

	void Material::SetTextureScale(const DirectX::XMFLOAT2& scale)
	{
		m_Properties.textureScale = scale;
		m_PropertiesDirty = true;
	}

	void Material::SetTextureOffset(const DirectX::XMFLOAT2& offset)
	{
		m_Properties.textureOffset = offset;
		m_PropertiesDirty = true;
	}

	void Material::SetDiffuseTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.diffuseTexture = texture;
		SetFlag(MaterialFlags::HasDiffuseTexture, texture != nullptr);
		UpdateTextureFlags();
		m_PropertiesDirty = true;
	}

	void Material::SetNormalTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.normalTexture = texture;
		SetFlag(MaterialFlags::HasNormalMap, texture != nullptr);
		UpdateTextureFlags();
		m_PropertiesDirty = true;
	}

	void Material::SetSpecularTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.specularTexture = texture;
		SetFlag(MaterialFlags::HasSpecularMap, texture != nullptr);
		UpdateTextureFlags();
		m_PropertiesDirty = true;
	}

	void Material::SetEmissiveTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.emissiveTexture = texture;
		SetFlag(MaterialFlags::HasEmissiveMap, texture != nullptr);
		UpdateTextureFlags();
		m_PropertiesDirty = true;

	}

	void Material::SetRoughnessTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.roughnessTexture = texture;
		SetFlag(MaterialFlags::HasRoughnessMap, texture != nullptr);
		UpdateTextureFlags();
		m_PropertiesDirty = true;
	}

	void Material::SetMetallicTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.metallicTexture = texture;
		SetFlag(MaterialFlags::HasMetallicMap, texture != nullptr);
		UpdateTextureFlags();
		m_PropertiesDirty = true;
	}

	void Material::SetAOTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.aoTexture = texture;
		SetFlag(MaterialFlags::HasAOMap, texture != nullptr);
		UpdateTextureFlags();
		m_PropertiesDirty = true;
	}

	void Material::SetHeightTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.heightTexture = texture;
		SetFlag(MaterialFlags::HasHeightMap, texture != nullptr);
		if (texture)
		{
			SetFlag(MaterialFlags::UseParallaxMapping, true);
		}
		UpdateTextureFlags();
		m_PropertiesDirty = true;
	}

	void Material::SetOpacityTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.opacityTexture = texture;
		SetFlag(MaterialFlags::HasOpacityMap, texture != nullptr);
		if (texture)
		{
			SetFlag(MaterialFlags::IsTransparent, true);
			if (m_RenderQueue == RenderQueue::Opaque)
			{
				m_RenderQueue = RenderQueue::Transparent;
			}
		}
		UpdateTextureFlags();
		m_PropertiesDirty = true;
	}

	void Material::SetDetailDiffuseTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.detailDiffuseTexture = texture;
		SetFlag(MaterialFlags::HasDetailMap, texture != nullptr);
		SetFlag(MaterialFlags::UseDetailTextures, texture != nullptr);
		UpdateTextureFlags();
		m_PropertiesDirty = true;
	}

	void Material::SetDetailNormalTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.detailNormalTexture = texture;
		SetFlag(MaterialFlags::UseDetailTextures, texture != nullptr);
		UpdateTextureFlags();
		m_PropertiesDirty = true;
	}

	void Material::SetEnvironmentTexture(std::shared_ptr<CubeMapTexture> texture)
	{
		m_Resources.environmentTexture = texture;
		SetFlag(MaterialFlags::HasEnvironmentMap, texture != nullptr);
		UpdateTextureFlags();
		m_PropertiesDirty = true;

	}

	void Material::SetFlag(MaterialFlags flag, bool enabled)
	{
		if (enabled)
		{
			m_Properties.flags |= flag;
		}
		else
		{
			m_Properties.flags &= ~flag;

		}
		m_PropertiesDirty = true;

	}

	bool Material::HasFlag(MaterialFlags flag) const
	{
		return (m_Properties.flags & flag) != 0;
	}

	void Material::UpdateTextureFlags()
	{
		m_PropertiesDirty = true;
	}

	void Material::InitializeConstantBuffer()
	{
		if (!m_ConstantBufferInitialized)
		{
			m_ConstantBuffer.Initialize(&m_Properties);
			m_ConstantBufferInitialized = true;
		}
	}

	void Material::UpdateConstantBuffer()
	{
		if (!m_ConstantBufferInitialized)
		{
			InitializeConstantBuffer();
		}

		m_ConstantBuffer.Update(m_Properties);
	}

	std::string Material::GetDebugInfo() const
	{
		std::string info = "Material: " + m_Name + "\n";
		info += "Type: " + std::to_string(static_cast<int>(m_Type)) + "\n";
		info += "Queue: " + std::to_string(static_cast<int>(m_RenderQueue)) + "\n";
		info += "Flags: " + std::to_string(m_Properties.flags) + "\n";
		return info;
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
		auto material = std::make_shared<Material>(name, MaterialType::UI);
		material->SetRenderQueue(RenderQueue::UI);
		material->SetFlag(MaterialFlags::IsTransparent, true);
		material->SetFlag(MaterialFlags::IsTwoSided, true);
		return material;
	}

	std::shared_ptr<Material> MaterialFactory::CreateFromConfig(const std::string& configPath)
	{
		// TODO: Implement material loading from file (JSON, XML, etc.)
		return CreateLitMaterial("DefaultFromConfig");
	}
	void Material::SetDetailTextureScale(const DirectX::XMFLOAT2& scale)
	{
		m_Properties.detailScale = scale;
		m_PropertiesDirty = true;
	}
	void Material::SetDetailTextureOffset(const DirectX::XMFLOAT2& offset)
	{
		m_Properties.detailOffset = offset;
		m_PropertiesDirty = true;
	}
	size_t Material::GetTextureCount() const
	{
		return m_Resources.GetTextureCount();
	}
	std::vector<std::shared_ptr<Texture>> Material::GetAllTextures() const
	{
		std::vector<std::shared_ptr<Texture>> textures;

		if (m_Resources.diffuseTexture) textures.push_back(m_Resources.diffuseTexture);
		if (m_Resources.normalTexture) textures.push_back(m_Resources.normalTexture);
		if (m_Resources.specularTexture) textures.push_back(m_Resources.specularTexture);
		if (m_Resources.emissiveTexture) textures.push_back(m_Resources.emissiveTexture);
		if (m_Resources.roughnessTexture) textures.push_back(m_Resources.roughnessTexture);
		if (m_Resources.metallicTexture) textures.push_back(m_Resources.metallicTexture);
		if (m_Resources.aoTexture) textures.push_back(m_Resources.aoTexture);
		if (m_Resources.heightTexture) textures.push_back(m_Resources.heightTexture);
		if (m_Resources.opacityTexture) textures.push_back(m_Resources.opacityTexture);
		if (m_Resources.detailDiffuseTexture) textures.push_back(m_Resources.detailDiffuseTexture);
		if (m_Resources.detailNormalTexture) textures.push_back(m_Resources.detailNormalTexture);

		return textures;
	}
	bool Material::IsTextureSlotUsed(TextureSlot slot) const
	{
		switch (slot)
		{
		case TextureSlot::Diffuse: return m_Resources.diffuseTexture != nullptr;
		case TextureSlot::Normal: return m_Resources.normalTexture != nullptr;
		case TextureSlot::Specular: return m_Resources.specularTexture != nullptr;
		case TextureSlot::Emissive: return m_Resources.emissiveTexture != nullptr;
		case TextureSlot::Roughness: return m_Resources.roughnessTexture != nullptr;
		case TextureSlot::Metallic: return m_Resources.metallicTexture != nullptr;
		case TextureSlot::AmbientOcclusion: return m_Resources.aoTexture != nullptr;
		case TextureSlot::Height: return m_Resources.heightTexture != nullptr;
		case TextureSlot::Opacity: return m_Resources.opacityTexture != nullptr;
		case TextureSlot::DetailDiffuse: return m_Resources.detailDiffuseTexture != nullptr;
		case TextureSlot::DetailNormal: return m_Resources.detailNormalTexture != nullptr;
		case TextureSlot::Environment: return m_Resources.environmentTexture != nullptr;
		default:
			return false;
		}
	}
	bool Material::ValidateTextures() const
	{
		auto textures = GetAllTextures();
		for (const auto& texture : textures)
		{
			if (texture && !texture->IsValid())
			{
				return false;
			}
		}
		return true;
	}
	std::string Material::GetTextureInfo() const
	{
		std::ostringstream info;
		info << "Material '" << m_Name << "' Texture Info:\n";
		info << "  Total Textures: " << GetTextureCount() << "\n";

		if (m_Resources.diffuseTexture) info << "  - Diffuse: Present\n";
		if (m_Resources.normalTexture) info << "  - Normal: Present\n";
		if (m_Resources.specularTexture) info << "  - Specular: Present\n";
		if (m_Resources.roughnessTexture) info << "  - Roughness: Present\n";
		if (m_Resources.metallicTexture) info << "  - Metallic: Present\n";
		if (m_Resources.emissiveTexture) info << "  - Emissive: Present\n";
		if (m_Resources.aoTexture) info << "  - AO: Present\n";
		if (m_Resources.heightTexture) info << "  - Height: Present\n";
		if (m_Resources.opacityTexture) info << "  - Opacity: Present\n";
		if (m_Resources.detailDiffuseTexture) info << "  - Detail Diffuse: Present\n";
		if (m_Resources.detailNormalTexture) info << "  - Detail Normal: Present\n";

		return info.str();
	}
}