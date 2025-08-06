#pragma once
#include "renderer/RendererCommand.h"
#include <wrl.h>

namespace DXEngine {

	class InputLayout
	{
	public:

		InputLayout(Microsoft::WRL::ComPtr<ID3DBlob> pVsBytecode);
		~InputLayout();

		void Bind();

	private:
		HRESULT hr;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> pInputLayout;
	};
}
