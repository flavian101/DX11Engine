#include "ShaderProgram.h"

ShaderProgram::ShaderProgram(Graphics& gfx, LPCWSTR vertexShader, LPCWSTR pixelShader)
	
{
	m_VertexShader = std::make_shared<VertexShader>(gfx, vertexShader);
	layout = std::make_shared<InputLayout>(gfx, m_VertexShader->GetByteCode());

	m_PixelShader = std::make_shared<PixelShader>(gfx, pixelShader);
}

ShaderProgram::~ShaderProgram()
{

}
void ShaderProgram::Bind(Graphics& gfx)
{
	layout->Bind(gfx);
	m_VertexShader->Bind(gfx);
	m_PixelShader->Bind(gfx);
}

ID3DBlob* ShaderProgram::GetByteCode()
{
	return m_VertexShader->GetByteCode();
}
