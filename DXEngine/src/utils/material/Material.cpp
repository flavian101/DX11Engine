#include "dxpch.h"
#include "Material.h"
#include <algorithm>

namespace DXEngine {

	uint32_t Material::s_NextID = 1;

	Material::Material(const std::string& name, ShadingModel model)
		: m_Name(name)
		, m_ID(s_NextID++)
		, m_Model(model)
		, m_Textures(kTextureSlotCount, nullptr)
	{}

	Material::~Material()
	{}

	void Material::SetTexture(TextureSlot slot, std::shared_ptr < RHI::ITexture> texture)
	{
		if (!MaterialSlots::IsValid(slot))
		{
			OutputDebugStringA(("Material::SetTexture: invalid texture slot " + std::to_string(static_cast<uint32_t>(slot)) + " ignored\n").c_str());
			return;
		}

		// The mask is a function of the textures: set while a texture is bound, clear when it is removed (setting nullptr
		// must switch the shader feature OFF, or the variant would still sample a texture that is gone). There is no
		// per-slot switch any more: the slot table decides what each bit means.
		m_Textures[static_cast<size_t>(slot)] = texture;
		if (texture) m_TextureMask |= MaterialSlots::MaskBit(slot);
		else         m_TextureMask &= ~MaterialSlots::MaskBit(slot);

		m_Dirty = true;
	}
	std::shared_ptr<RHI::ITexture> Material::GetTexture(TextureSlot slot)
	{
		return MaterialSlots::IsValid(slot) ? m_Textures[static_cast<size_t>(slot)] : nullptr;
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
		auto mat = std::make_shared<Material>(name, ShadingModel::Unlit);
		mat->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateLitMaterial(const std::string& name)
	{
		auto mat = std::make_shared<Material>(name, ShadingModel::Lit);
		mat->SetDiffuseColor({ 0.8f, 0.8f, 0.8f, 1.0f });
		mat->SetSpecularColor({ 0.5f, 0.5f, 0.5f, 1.0f });
		mat->SetShininess(32.0f);
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreatePBRMaterial(const std::string& name)
	{
		auto mat = CreateLitMaterial(name);     // PBR IS the Lit shading model
		mat->SetMetallic(0.0f);
		mat->SetRoughness(0.5f);
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateEmissiveMaterial(const std::string& name)
	{
		auto mat = std::make_shared<Material>(name, ShadingModel::Emissive);
		mat->SetEmissiveColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		mat->SetEmissiveIntensity(3.0f);
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateSkyboxMaterial(const std::string& name)
	{
		auto mat = std::make_shared<Material>(name, ShadingModel::Sky);
		mat->SetRenderState(RenderState::Sky());
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateTransparentMaterial(const std::string& name)
	{
		auto mat = CreateLitMaterial(name);
		mat->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 0.5f });
		mat->SetRenderState(RenderState::Blended());
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateUIMaterial(const std::string& name)
	{
		auto mat = std::make_shared<Material>(name, ShadingModel::UI);
		mat->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		mat->SetRenderState(RenderState::Overlay());
		return mat;
	}

	std::shared_ptr<Material> MaterialFactory::CreateFromConfig(const std::string& configPath)
	{
		// TODO: Implement material loading from file (JSON, XML, etc.)
		return CreateLitMaterial("DefaultFromConfig");
	}

}