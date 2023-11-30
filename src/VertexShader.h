#pragma once
#include "Graphics.h"
#include <wrl.h>

class VertexShader
{
public:
	VertexShader(Graphics& g, LPCWSTR filename);
	void Bind(Graphics &g);

	ID3DBlob* GetByteCode();

private:
	//void PrintError(HRESULT vhr);

private:
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> pVertexShader;
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;

};

