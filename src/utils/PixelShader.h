#pragma once
#include "renderer/RendererCommand.h"
#include <wrl.h>

namespace DXEngine {


	class PixelShader
	{
	public:

		PixelShader(LPCWSTR filename);
		~PixelShader();
		void Bind();

	private:
		HRESULT hr;
		Microsoft::WRL::ComPtr<ID3DBlob> pShaderBlob;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> pPixelShader;

	};

}