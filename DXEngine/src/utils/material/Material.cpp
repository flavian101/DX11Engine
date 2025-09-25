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
			SetFlag(MaterialFlags::castsShadows, false);
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

		UpdateFlags();
	}

	Material::~Material()
	{
	}
	void Material::Bind()
	{
		if (!IsValid())
		{
			return;
		}
		if (m_PropertiesDirty)
		{
			UpdateConstantBuffer();
			m_PropertiesDirty = false;
		}

		if (m_ConstantBufferInitialized)
		{
			RenderCommand::GetContext()->PSSetConstantBuffers(BindSlot::CB_Material, 1, m_ConstantBuffer.GetAddressOf());
		}

		if (HasFlag(MaterialFlags::HasDiffuseTexture) && m_Resources.diffuseTexture)
		{
			m_Resources.diffuseTexture->Bind(static_cast<UINT>(TextureSlot::Diffuse));
		}

		if (HasFlag(MaterialFlags::HasNormalMap) && m_Resources.normalTexture)
		{
			m_Resources.normalTexture->Bind(static_cast<UINT>(TextureSlot::Normal));
		}

		if (HasFlag(MaterialFlags::HasSpecularMap) && m_Resources.specularTexture)
		{
			m_Resources.specularTexture->Bind(static_cast<UINT>(TextureSlot::Specular));
		}

		if (HasFlag(MaterialFlags::HasEmissiveMap) && m_Resources.emissiveTexture)
		{
			m_Resources.emissiveTexture->Bind(static_cast<UINT>(TextureSlot::Emissive));
		}

		if (m_Resources.environmentTexture)
		{
			m_Resources.environmentTexture->Bind(static_cast<UINT>(TextureSlot::Environment));
		}
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
		UpdateFlags();
	}

	void Material::SetNormalTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.normalTexture = texture;
		SetFlag(MaterialFlags::HasNormalMap, texture != nullptr);
		UpdateFlags();
	}

	void Material::SetSpecularTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.specularTexture = texture;
		SetFlag(MaterialFlags::HasSpecularMap, texture != nullptr);
		UpdateFlags();
	}

	void Material::SetEmissiveTexture(std::shared_ptr<Texture> texture)
	{
		m_Resources.emissiveTexture = texture;
		SetFlag(MaterialFlags::HasEmissiveMap, texture != nullptr);
		UpdateFlags();
	}

	void Material::SetEnvironmentTexture(std::shared_ptr<CubeMapTexture> texture)
	{
		m_Resources.environmentTexture = texture;
		UpdateFlags();
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

	void Material::UpdateFlags()
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
}