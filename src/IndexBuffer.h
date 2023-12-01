#pragma once
#include "Graphics.h"
#include "wrl.h"


class IndexBuffer
{
public:
	IndexBuffer(Graphics& g, const unsigned short indices[], UINT indexCount);

	void Bind(Graphics& g);

private:
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11Buffer> pIndexBuffer;
};

