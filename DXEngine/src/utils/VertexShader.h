#pragma once
#include "renderer/RendererCommand.h"
#include <wrl.h>

namespace DXEngine {

	class VertexShader
	{
	public:
		VertexShader( LPCWSTR filename);
		VertexShader(ID3DBlob* shaderBlob);

		~VertexShader();
		void Bind();

		ID3DBlob* GetByteCode();

	private:
		//void PrintError(HRESULT vhr);

	private:
		HRESULT hr;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_pVertexShader;
		Microsoft::WRL::ComPtr<ID3DBlob> m_ShaderByteCode;

	};
}

