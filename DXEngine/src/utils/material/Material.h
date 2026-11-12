#pragma once
#include "MaterialTypes.h"
#include "MaterialProperties.h"
#include <string>
#include <memory>
#include "RHI/GraphicsDevice.h"

namespace DXEngine {

	class ShaderProgram;

	class Material
	{
	public:
		Material(const std::string& name = "DefaultMaterial", ShadingModel model = ShadingModel::Lit);
		~Material();

		// WHAT the pixels compute. Fixed for the life of the material: no setter changes it behind your back (Material used to
		// turn itself from Lit into PBR when you set a metallic or roughness value).
		ShadingModel GetShadingModel() const { return m_Model; }

		const std::string& GetName()const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }
		
		//Material Properties
		MaterialProperties& GetProperties() { return m_Properties; }
		const MaterialProperties& GetProperties() const { return m_Properties; }

		//setTextures
		void SetTexture(TextureSlot slot, std::shared_ptr <RHI::ITexture> texture);
		std::shared_ptr<RHI::ITexture> GetTexture(TextureSlot slot);
		const std::vector<std::shared_ptr<RHI::ITexture>>& GetTextures() const { return m_Textures; }   // index == TextureSlot == register

		// Bit i set = a texture is bound in slot i. DERIVED from the textures themselves (SetTexture is its only writer), so
		// it cannot disagree with them. The shader variant is chosen from this (see MaterialSlots.h).
		uint32_t GetTextureMask() const { return m_TextureMask; }

		// ========== Constant Buffer ==========
		RHI::IBuffer* GetConstantBuffer() { return m_ConstantBuffer.get(); }
		void UpdateConstantBuffer(RHI::IGraphicsDevice* device);

		// ========== Render State: HOW it is drawn (independent of the shading model) ==========
		RenderState&       GetRenderState()       { return m_State; }
		const RenderState& GetRenderState() const { return m_State; }
		void SetRenderState(const RenderState& state) { m_State = state; }

		// Derived from the state, never stored: the draw-order bucket and "is this surface alpha blended".
		RenderQueue GetRenderQueue() const { return ResolveRenderQueue(m_State); }
		bool        IsBlended() const      { return DXEngine::IsBlended(m_State); }

		// Single-field shortcuts (the state is the source of truth)
		void SetBlendMode(RHI::BlendMode mode) { m_State.blend = mode; }
		RHI::BlendMode GetBlendMode() const { return m_State.blend; }
		void SetCullMode(RHI::CullMode mode) { m_State.cull = mode; }
		RHI::CullMode GetCullMode() const { return m_State.cull; }
		void SetDepthTest(RHI::DepthTestMode mode) { m_State.depthTest = mode; }
		RHI::DepthTestMode GetDepthTest() const { return m_State.depthTest; }

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
		ShadingModel m_Model;
		MaterialProperties m_Properties;
		std::vector<std::shared_ptr<RHI::ITexture>> m_Textures;
		std::shared_ptr<RHI::IBuffer> m_ConstantBuffer;

		uint32_t m_TextureMask = 0;      // derived from m_Textures, see GetTextureMask()
		bool m_Dirty = true;

		RenderState m_State;             // see RenderState: the pipeline, the draw order and the shadow pass all read this

		static uint32_t s_NextID;
	};


	class MaterialFactory
	{
	public:
		static std::shared_ptr<Material> CreateUnlitMaterial(const std::string& name = "Unlit");
		static std::shared_ptr<Material> CreateLitMaterial(const std::string& name = "Lit");
		// PBR is the Lit shading model: this is CreateLitMaterial with metallic / roughness set (kept for existing callers).
		static std::shared_ptr<Material> CreatePBRMaterial(const std::string& name = "PBR");
		static std::shared_ptr<Material> CreateEmissiveMaterial(const std::string& name = "Emissive");
		static std::shared_ptr<Material> CreateSkyboxMaterial(const std::string& name = "Skybox");
		// Transparent = the Lit shading model + a blended RenderState (there is no separate "transparent" shader).
		static std::shared_ptr<Material> CreateTransparentMaterial(const std::string& name = "Transparent");
		static std::shared_ptr<Material> CreateUIMaterial(const std::string& name = "UI");


		// Create material from config
		static std::shared_ptr<Material> CreateFromConfig(const std::string& configPath);
	};

}