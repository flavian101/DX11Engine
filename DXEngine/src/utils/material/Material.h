#pragma once
#include "MaterialTypes.h"
#include "MaterialProperties.h"
#include "utils/Sampler.h"
#include <string>
#include <memory>
#include "RHI/GraphicsDevice.h"

namespace DXEngine {

	class ShaderProgram;


	class Material
	{
	public:
		Material(const std::string& name = "DefaultMaterial", MaterialType type = MaterialType::Lit);
		~Material();

		//material type and Properties 
		MaterialType GetType()const { return m_Type; }

		const std::string& GetName()const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }
		
		//Material Properties
		MaterialProperties& GetProperties() { return m_Properties; }
		const MaterialProperties& GetProperties() const { return m_Properties; }

		//setTextures
		void SetTexture(TextureSlot slot, RHI::ITexture* texture);
		RHI::ITexture* GetTexture(TextureSlot slot);
		const std::vector<RHI::ITexture*>& GetTextures() const { return m_Textures; }

		// ========== Constant Buffer ==========
		RHI::IBuffer* GetConstantBuffer() { return m_ConstantBuffer.get(); }
		void UpdateConstantBuffer(RHI::IGraphicsDevice* device);

		// ========== Features ==========
		uint32_t GetFeatureFlags() const { return m_FeatureFlags; }
		void EnableFeature(uint32_t feature) { m_FeatureFlags |= feature; }
		void DisableFeature(uint32_t feature) { m_FeatureFlags &= ~feature; }
		bool HasFeature(uint32_t feature) const { return (m_FeatureFlags & feature) != 0; }

		// ========== Render State ==========
		void SetBlendMode(RHI::BlendMode mode) { m_BlendMode = mode; }
		RHI::BlendMode GetBlendMode() const { return m_BlendMode; }

		void SetCullMode(RHI::CullMode mode) { m_CullMode = mode; }
		RHI::CullMode GetCullMode() const { return m_CullMode; }

		void SetDepthTest(RHI::DepthTestMode mode) { m_DepthTest = mode; }
		RHI::DepthTestMode GetDepthTest() const { return m_DepthTest; }

		// ========== Shader Selection ==========
		std::string GetShaderName() const;  // Determined by type

		void SetDiffuseColor(const DirectX::XMFLOAT4& color);
		void SetSpecularColor(const DirectX::XMFLOAT4& color);
		void SetEmissiveColor(const DirectX::XMFLOAT4& color);
		void SetShininess(float shininess);
		void SetAlpha(float alpha);
		void SetMetallic(float metallic);
		void SetRoughness(float roughness);
		void SetNormalScale(float scale);
		void SetHeightScale(float scale);
		void SetOcculsionStrength(float strength);
		void SetEmissiveIntensity(float intensity);
		void SetTextureScale(const DirectX::XMFLOAT2& scale);
		void SetTextureOffset(const DirectX::XMFLOAT2& offset);

		float GetMetallic() const { return m_Properties.metallic; }
		float GetRoughness() const { return m_Properties.roughness; }
		float GetNormalScale() const { return m_Properties.normalScale; }
		float GetHeightScale() const { return m_Properties.heightScale; }

		// Texture configuration
		void SetDetailTextureScale(const DirectX::XMFLOAT2& scale);
		void SetDetailTextureOffset(const DirectX::XMFLOAT2& offset);

	private:
		std::string m_Name;
		uint32_t m_ID;
		MaterialType m_Type;
		RenderQueue m_RenderQueue;
		MaterialProperties m_Properties;
		std::vector<RHI::ITexture*> m_Textures;
		std::shared_ptr<RHI::IBuffer> m_ConstantBuffer;

		uint32_t m_FeatureFlags = 0;
		bool m_Dirty = true;


		// Render state
		RHI::BlendMode m_BlendMode = RHI::BlendMode::Opaque;
		RHI::CullMode m_CullMode = RHI::CullMode::Back;
		RHI::DepthTestMode m_DepthTest = RHI::DepthTestMode::Less;

		static uint32_t s_NextID;
	};


	class MaterialFactory
	{
	public:
		static std::shared_ptr<Material> CreateUnlitMaterial(const std::string& name = "Unlit");
		static std::shared_ptr<Material> CreateLitMaterial(const std::string& name = "Lit");
		static std::shared_ptr<Material> CreatePBRMaterial(const std::string& name = "PBR");
		static std::shared_ptr<Material> CreateEmissiveMaterial(const std::string& name = "Emissive");
		static std::shared_ptr<Material> CreateSkyboxMaterial(const std::string& name = "Skybox");
		static std::shared_ptr<Material> CreateTransparentMaterial(const std::string& name = "Transparent");
		static std::shared_ptr<Material> CreateUIMaterial(const std::string& name = "UI");


		// Create material from config
		static std::shared_ptr<Material> CreateFromConfig(const std::string& configPath);
	};

}