#pragma once
#include "MaterialTypes.h"
#include "MaterialProperties.h"
#include "utils/ConstantBuffer.h"
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
		void SetTextureScale(const DirectX::XMFLOAT2& scale);
		void SetTextureOffset(const DirectX::XMFLOAT2& offset);

		void SetDiffuseTexture(std::shared_ptr<Texture> texture);
		void SetNormalTexture(std::shared_ptr<Texture> texture);
		void SetSpecularTexture(std::shared_ptr<Texture> texture);
		void SetEmissiveTexture(std::shared_ptr<Texture> texture);
		void SetEnvironmentTexture(std::shared_ptr<CubeMapTexture> texture);


		void SetFlag(MaterialFlags flag, bool enabled);
		bool HasFlag(MaterialFlags flag) const;

		std::string GetDebugInfo() const;

	private:
		void UpdateFlags();
		void InitializeConstantBuffer();
		void UpdateConstantBuffer();


	private:
		std::string m_Name;
		MaterialType m_Type;
		RenderQueue m_RenderQueue;
		MaterialProperties m_Properties;
		MaterialResources m_Resources;
		ConstantBuffer<MaterialProperties> m_ConstantBuffer;
		Sampler m_Sampler;
		bool m_ConstantBufferInitialized = false;
		bool m_PropertiesDirty = true;
	};


	class MaterialFactory
	{
	public:
		static std::shared_ptr<Material> CreateUnlitMaterial(const std::string& name = "Unlit");
		static std::shared_ptr<Material> CreateLitMaterial(const std::string& name = "Lit");
		static std::shared_ptr<Material> CreateTexturedMaterial(const std::string& name = "Textured");
		static std::shared_ptr<Material> CreateTexturedNormalMaterial(const std::string& name);
		static std::shared_ptr<Material> CreateSkyboxMaterial(const std::string& name = "Skybox");
		static std::shared_ptr<Material> CreateTransparentMaterial(const std::string& name = "Transparent");

		// Create material from config
		static std::shared_ptr<Material> CreateFromConfig(const std::string& configPath);
	};

}