#pragma once
#include "renderer/RendererCommand.h"
#include <wrl.h>

namespace DXEngine {

	class VertexShader
	{
	public:
		VertexShader( LPCWSTR filename);
		~VertexShader();
		void Bind();

		ID3DBlob* GetByteCode();

	private:
		//void PrintError(HRESULT vhr);

	private:
		HRESULT hr;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> pVertexShader;
		Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;

	};
}

