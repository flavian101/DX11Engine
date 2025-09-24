#include "dxpch.h"
#include "PixelShader.h"

namespace DXEngine {

	PixelShader::PixelShader(LPCWSTR filename)
	{
		//hr = D3DCompileFromFile(
		//	filename, nullptr, nullptr, "main", "ps_5_0", 0, 0, &pShaderBlob, NULL);
		D3DReadFileToBlob(filename, &m_ShaderByteCode);
		RenderCommand:: GetDevice()->CreatePixelShader(m_ShaderByteCode->GetBufferPointer(), m_ShaderByteCode->GetBufferSize(), NULL, &m_pPixelShader);
	}

	PixelShader::PixelShader(ID3DBlob* shaderBlob)
	{
		HRESULT hr = RenderCommand::GetDevice()->CreatePixelShader(
			shaderBlob->GetBufferPointer(),
			shaderBlob->GetBufferSize(),
			nullptr,
			&m_pPixelShader
		);

		if (FAILED(hr)) {
			throw std::runtime_error("Failed to create pixel shader from blob");
		}
	}

	PixelShader::~PixelShader()
	{
		m_ShaderByteCode.Reset();
		m_pPixelShader.Reset();
	}

	void PixelShader::Bind()
	{
		RenderCommand::GetContext()->PSSetShader(m_pPixelShader.Get(), nullptr, 0);
	}
}