#pragma once
#include "renderer/RendererCommand.h"
#include <wrl.h>
#include"Vertex.h"

namespace DXEngine {

	class VertexBuffer
	{
	public:
		VertexBuffer( const std::vector< Vertex>& v);
		~VertexBuffer();
		void Bind();


	private:
		HRESULT hr;
		UINT stride;
		Microsoft::WRL::ComPtr<ID3D11Buffer> pVertexBuffer;
	};

}