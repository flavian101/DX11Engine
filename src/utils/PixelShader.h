#pragma once
#include "Graphics.h"
#include <wrl.h>

namespace DXEngine {


	class PixelShader
	{
	public:

		PixelShader(Graphics& g, LPCWSTR filename);
		~PixelShader();
		void Bind(Graphics& g);

	private:
		HRESULT hr;
		Microsoft::WRL::ComPtr<ID3DBlob> pShaderBlob;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> pPixelShader;

	};

}