#include "ShaderProgram.h"
namespace DXEngine {

	ShaderProgram::ShaderProgram( LPCWSTR vertexShader, LPCWSTR pixelShader)

	{
		m_VertexShader = std::make_shared<VertexShader>(vertexShader);
		layout = std::make_shared<InputLayout>(m_VertexShader->GetByteCode());

		m_PixelShader = std::make_shared<PixelShader>(pixelShader);
	}

	ShaderProgram::~ShaderProgram()
	{

	}
	void ShaderProgram::Bind()
	{
		layout->Bind();
		m_VertexShader->Bind();
		m_PixelShader->Bind();
	}

	ID3DBlob* ShaderProgram::GetByteCode()
	{
		return m_VertexShader->GetByteCode();
	}
}