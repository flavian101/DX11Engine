#pragma once
#include "Graphics.h"
#include <wrl.h>
#include"Vertex.h"

class VertexBuffer
{
public:
	VertexBuffer(Graphics& g, Vertex* v, UINT dataSize);

	void Bind(Graphics& g);


private:
	HRESULT hr;
	UINT stride;
	Microsoft::WRL::ComPtr<ID3D11Buffer> pVertexBuffer;
};

