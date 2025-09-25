#include "dxpch.h"
#include "ShaderProgram.h"
namespace DXEngine {

	ShaderProgram::ShaderProgram( LPCWSTR vertexShader, LPCWSTR pixelShader)

	{
		m_VertexShader = std::make_shared<VertexShader>(vertexShader);
		m_PixelShader = std::make_shared<PixelShader>(pixelShader);
	}

	ShaderProgram::ShaderProgram(std::shared_ptr<VertexShader> vs, std::shared_ptr<PixelShader> ps)
		: m_VertexShader(vs), m_PixelShader(ps)
	{
	}

	ShaderProgram::ShaderProgram(Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, Microsoft::WRL::ComPtr<ID3DBlob> psBlob)
	{
		if (vsBlob) {
			m_VertexShader = std::make_shared<VertexShader>(vsBlob.Get());
		}
		if (psBlob) {
			m_PixelShader = std::make_shared<PixelShader>(psBlob.Get());
		}
	}

	ShaderProgram::~ShaderProgram()
	{

	}
	void ShaderProgram::Bind()
	{
		if (m_VertexShader) m_VertexShader->Bind();
		if (m_PixelShader) m_PixelShader->Bind();

	}

	ID3DBlob* ShaderProgram::GetByteCode()
	{
		return m_VertexShader ? m_VertexShader->GetByteCode() : nullptr;
	}
}