#pragma once
#include "Graphics.h"
#include "wrl.h"


class IndexBuffer
{
	IndexBuffer(Graphics& g, const unsigned short indices[]);

	void Bind(Graphics& g);

private:
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11Buffer> pIndexBuffer;
};

