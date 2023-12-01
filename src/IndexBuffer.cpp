#include "IndexBuffer.h"

IndexBuffer::IndexBuffer(Graphics& g, const unsigned short indices[])
{
	D3D11_BUFFER_DESC idDesc;
    ZeroMemory(&idDesc, sizeof(idDesc));

    idDesc.Usage = D3D11_USAGE_DEFAULT;
    idDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    idDesc.CPUAccessFlags = 0;
    idDesc.MiscFlags = 0;
    idDesc.ByteWidth = sizeof(indices)* sizeof(unsigned short);
    idDesc.StructureByteStride = sizeof(unsigned short);

    D3D11_SUBRESOURCE_DATA idData;
    idData.pSysMem = indices;

    hr = g.GetDevice()->CreateBuffer(&idDesc, &idData,pIndexBuffer.GetAddressOf());

}

void IndexBuffer::Bind(Graphics& g)
{
    g.GetContext()->IASetIndexBuffer(pIndexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
}
