#include "PixelShader.h"

namespace DXEngine {

	PixelShader::PixelShader(Graphics& g, LPCWSTR filename)
	{
		//hr = D3DCompileFromFile(
		//	filename, nullptr, nullptr, "main", "ps_5_0", 0, 0, &pShaderBlob, NULL);
		D3DReadFileToBlob(filename, &pShaderBlob);
		g.GetDevice()->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &pPixelShader);
	}

	PixelShader::~PixelShader()
	{
		pShaderBlob.Reset();
		pPixelShader.Reset();
	}

	void PixelShader::Bind(Graphics& g)
	{
		g.GetContext()->PSSetShader(pPixelShader.Get(), nullptr, 0);
	}
}