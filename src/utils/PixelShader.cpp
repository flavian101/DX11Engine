#include "PixelShader.h"

namespace DXEngine {

	PixelShader::PixelShader(LPCWSTR filename)
	{
		//hr = D3DCompileFromFile(
		//	filename, nullptr, nullptr, "main", "ps_5_0", 0, 0, &pShaderBlob, NULL);
		D3DReadFileToBlob(filename, &pShaderBlob);
		RenderCommand:: GetDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &pPixelShader);
	}

	PixelShader::~PixelShader()
	{
		pShaderBlob.Reset();
		pPixelShader.Reset();
	}

	void PixelShader::Bind()
	{
		RenderCommand::GetContext()->PSSetShader(pPixelShader.Get(), nullptr, 0);
	}
}