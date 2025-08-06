#include "Material.h"
namespace DXEngine {

	Material::Material()
		: samp()
	{
		psBuffer.Initialize();
		samp.Bind();
		psBuffer.data.difColor = { 1.0f,1.0f,1.0f,1.0f };
		psBuffer.data.hasTexture = static_cast<BOOL>(FALSE);
		psBuffer.data.hasNormalMap = static_cast<BOOL>(FALSE);
	}

	Material::~Material()
	{
	}
	void Material::Bind()
	{
		if (m_Program)
			m_Program->Bind();

		if (isCubemap)
		{
			m_CubeMap->Bind();
		}


		psBuffer.Update();

		if (psBuffer.data.hasTexture)
			m_Diffuse->Bind(Diffuse);
		if (psBuffer.data.hasNormalMap)
			m_NormalMap->Bind(NormalMap);

		RenderCommand::GetContext()->PSSetConstantBuffers(BindSlot::CB_Material, 1, psBuffer.GetAddressOf());

	}

	void Material::SetShaderProgram(std::shared_ptr<ShaderProgram> program)
	{
		m_Program = program;
	}

	void Material::SetDiffuseColor(const DirectX::XMFLOAT4& diffuse)
	{
		psBuffer.data.difColor = diffuse;
	}

	void Material::SetDiffuse(const std::shared_ptr<Texture>& diffuseTexture)
	{

		m_Diffuse = diffuseTexture;
		psBuffer.data.hasTexture = static_cast<BOOL>(TRUE);
	}

	void Material::SetNormalMap(const std::shared_ptr<Texture>& normalTexture)
	{
		m_NormalMap = normalTexture;
		psBuffer.data.hasNormalMap = static_cast<BOOL>(TRUE);

	}

	void Material::SetSkyMaterial(const std::shared_ptr<CubeMapTexture>& cubemap)
	{
		m_CubeMap = cubemap;
		isCubemap = true;
	}
}