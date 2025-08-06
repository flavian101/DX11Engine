#include "IndexBuffer.h"

namespace DXEngine {

    IndexBuffer::IndexBuffer(const std::vector<unsigned short>& indices)
    {
        D3D11_BUFFER_DESC idDesc;
        ZeroMemory(&idDesc, sizeof(idDesc));

        idDesc.Usage = D3D11_USAGE_DEFAULT;
        idDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        idDesc.CPUAccessFlags = 0;
        idDesc.MiscFlags = 0;
        idDesc.ByteWidth = indices.size() * sizeof(unsigned short);
        idDesc.StructureByteStride = sizeof(unsigned short);

        D3D11_SUBRESOURCE_DATA idData;
        idData.pSysMem = indices.data();

        hr = RenderCommand::GetDevice()->CreateBuffer(&idDesc, &idData, pIndexBuffer.GetAddressOf());

    }

    IndexBuffer::~IndexBuffer()
    {
        pIndexBuffer.Reset();
    }

    void IndexBuffer::Bind()
    {
        RenderCommand::GetContext()->IASetIndexBuffer(pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    }
}