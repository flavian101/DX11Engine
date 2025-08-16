#include "dxpch.h"
#include "ShaderProgram.h"
namespace DXEngine {

	ShaderProgram::ShaderProgram( LPCWSTR vertexShader, LPCWSTR pixelShader)

	{
		m_VertexShader = std::make_shared<VertexShader>(vertexShader);
		m_PixelShader = std::make_shared<PixelShader>(pixelShader);
	}

	ShaderProgram::~ShaderProgram()
	{

	}
	void ShaderProgram::Bind()
	{
		m_VertexShader->Bind();
		m_PixelShader->Bind();
	}

	ID3DBlob* ShaderProgram::GetByteCode()
	{
		return m_VertexShader->GetByteCode();
	}
}