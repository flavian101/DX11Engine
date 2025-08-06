#pragma once
#include "renderer/RendererCommand.h"
#include "wrl.h"
#include <vector>

namespace DXEngine {

	class IndexBuffer
	{
	public:
		IndexBuffer(const std::vector<unsigned short>& indices);
		~IndexBuffer();

		void Bind();

	private:
		HRESULT hr;
		Microsoft::WRL::ComPtr<ID3D11Buffer> pIndexBuffer;
	};

}