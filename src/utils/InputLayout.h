#pragma once
#include "Graphics.h"
#include <wrl.h>

namespace DXEngine {

	class InputLayout
	{
	public:

		InputLayout(Graphics& g, Microsoft::WRL::ComPtr<ID3DBlob> pVsBytecode);
		~InputLayout();

		void Bind(Graphics& g);

	private:
		HRESULT hr;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> pInputLayout;
	};
}
