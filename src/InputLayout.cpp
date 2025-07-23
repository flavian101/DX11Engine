#include "InputLayout.h"

InputLayout::InputLayout(Graphics& g, Microsoft::WRL::ComPtr<ID3DBlob> pVsBytecode)
{
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",	 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0}

	};
	UINT numElements = ARRAYSIZE(layout);

	hr = g.GetDevice()->CreateInputLayout(layout, numElements, pVsBytecode->GetBufferPointer()
		,pVsBytecode->GetBufferSize(), &pInputLayout);

}

InputLayout::~InputLayout()
{
	pInputLayout.Reset();
}

void InputLayout::Bind(Graphics& g)
{
	g.GetContext()->IASetInputLayout(pInputLayout.Get());
}
