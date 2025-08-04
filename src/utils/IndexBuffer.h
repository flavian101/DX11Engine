#pragma once
#include "Graphics.h"
#include "wrl.h"
#include <vector>

namespace DXEngine {

	class IndexBuffer
	{
	public:
		IndexBuffer(Graphics& g, const std::vector<unsigned short>& indices);
		~IndexBuffer();

		void Bind(Graphics& g);

	private:
		HRESULT hr;
		Microsoft::WRL::ComPtr<ID3D11Buffer> pIndexBuffer;
	};

}