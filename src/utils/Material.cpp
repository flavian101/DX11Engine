#include "Material.h"

Material::Material(Graphics& gfx)
	: samp(gfx)
{
	psBuffer.Initialize(gfx);
	samp.Bind(gfx);
	psBuffer.data.difColor = {1.0f,1.0f,1.0f,1.0f};
	psBuffer.data.hasTexture = static_cast<BOOL>(FALSE);
	psBuffer.data.hasNormalMap = static_cast<BOOL>(FALSE);
}

Material::~Material()
{
}
void Material::Bind(Graphics& gfx)
{
	if (m_Program)
		m_Program->Bind(gfx);

	if (isCubemap)
	{
		m_CubeMap->Bind(gfx);
	}


	psBuffer.Update(gfx);
	
	if (psBuffer.data.hasTexture)
		m_Diffuse->Bind(gfx,Diffuse);
	if (psBuffer.data.hasNormalMap)
		m_NormalMap->Bind(gfx,NormalMap);

	gfx.GetContext()->PSSetConstantBuffers(BindSlot::CB_Material, 1, psBuffer.GetAddressOf());

}

void Material::SetShaderProgram(std::shared_ptr<ShaderProgram> program)
{
	m_Program = program;
}

void Material::SetDiffuseColor(const XMFLOAT4& diffuse)
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
