#pragma once
#include "MaterialTypes.h"
#include "MaterialProperties.h"
#include "utils/Buffer.h"
#include "utils/Sampler.h"
#include <string>
#include <memory>

namespace DXEngine {

	class ShaderProgram;
	class Texture;
	class CubeMapTexture;

	class Material
	{
	public:
		Material(const std::string& name = "DefaultMaterial", MaterialType type = MaterialType::Lit);
		~Material();

		void Bind();
		bool IsValid() const;

		//material type and Properties 
		MaterialType GetType()const { return m_Type; }
		void SetType(MaterialType type);

		const std::string& GetName()const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		RenderQueue GetRenderQueue() const { return m_RenderQueue; }
		void SetRenderQueue(RenderQueue queue) { m_RenderQueue = queue; }
		
		//Material Properties
		MaterialProperties& GetProperties() { return m_Properties; }
		const MaterialProperties& GetProperties() const { return m_Properties; }

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

		void SetDiffuseTexture(std::shared_ptr<Texture> texture);
		void SetNormalTexture(std::shared_ptr<Texture> texture);
		void SetSpecularTexture(std::shared_ptr<Texture> texture);
		void SetEmissiveTexture(std::shared_ptr<Texture> texture);
		void SetRoughnessTexture(std::shared_ptr<Texture> texture);
		void SetMetallicTexture(std::shared_ptr<Texture> texture);
		void SetAOTexture(std::shared_ptr<Texture> texture);
		void SetHeightTexture(std::shared_ptr<Texture> texture);
		void SetOpacityTexture(std::shared_ptr<Texture> texture);
		void SetDetailDiffuseTexture(std::shared_ptr<Texture> texture);
		void SetDetailNormalTexture(std::shared_ptr<Texture> texture);
		void SetEnvironmentTexture(std::shared_ptr<CubeMapTexture> texture);

		//texture checking
		bool HasDiffuseTexture()const { return m_Resources.diffuseTexture != nullptr; }
		bool HasNormalTexture()const { return m_Resources.normalTexture != nullptr; }
		bool HasEmissiveTexture()const { return m_Resources.emissiveTexture != nullptr; }
		bool HasRoughnessTexture() const { return m_Resources.roughnessTexture != nullptr; }
		bool HasMetallicTexture() const { return m_Resources.metallicTexture != nullptr; }
		bool HasAOTexture() const { return m_Resources.aoTexture != nullptr; }
		bool HasHeightTexture() const { return m_Resources.heightTexture != nullptr; }
		bool HasOpacityTexture() const { return m_Resources.opacityTexture != nullptr; }

		std::shared_ptr<Texture> GetRoughnessTexture() const { return m_Resources.roughnessTexture; }
		std::shared_ptr<Texture> GetMetallicTexture() const { return m_Resources.metallicTexture; }
		std::shared_ptr<Texture> GetAOTexture() const { return m_Resources.aoTexture; }
		std::shared_ptr<Texture> GetHeightTexture() const { return m_Resources.heightTexture; }
		std::shared_ptr<Texture> GetOpacityTexture() const { return m_Resources.opacityTexture; }

		float GetMetallic() const { return m_Properties.metallic; }
		float GetRoughness() const { return m_Properties.roughness; }
		float GetNormalScale() const { return m_Properties.normalScale; }
		float GetHeightScale() const { return m_Properties.heightScale; }



		// Texture configuration
		void SetDetailTextureScale(const DirectX::XMFLOAT2& scale);
		void SetDetailTextureOffset(const DirectX::XMFLOAT2& offset);


		// Utility methods
		size_t GetTextureCount() const;
		std::vector<std::shared_ptr<Texture>> GetAllTextures() const;
		bool IsTextureSlotUsed(TextureSlot slot) const;
		bool ValidateTextures() const;
		std::string GetTextureInfo() const;


		void SetFlag(MaterialFlags flag, bool enabled);
		bool HasFlag(MaterialFlags flag) const;

		std::string GetDebugInfo() const;

	private:
		void UpdateTextureFlags();
		void InitializeConstantBuffer();
		void UpdateConstantBuffer();


	private:
		std::string m_Name;
		MaterialType m_Type;
		RenderQueue m_RenderQueue;
		MaterialProperties m_Properties;
		MaterialResources m_Resources;
		ConstantBuffer<MaterialProperties> m_ConstantBuffer;
		
		bool m_ConstantBufferInitialized = false;
		bool m_PropertiesDirty = true;
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