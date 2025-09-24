#pragma once
#include "renderer/RendererCommand.h"
#include <wrl.h>

namespace DXEngine {


	class PixelShader
	{
	public:

		PixelShader(LPCWSTR filename);
		PixelShader(ID3DBlob* shaderBlob);

		~PixelShader();
		void Bind();

	private:
		HRESULT hr;
		Microsoft::WRL::ComPtr<ID3DBlob> m_ShaderByteCode;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPixelShader;

	};

}