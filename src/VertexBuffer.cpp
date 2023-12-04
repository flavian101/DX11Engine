#include "VertexBuffer.h"
#include <vector>



VertexBuffer::VertexBuffer(Graphics& g, std::vector< Vertex> v)
    :
    stride(sizeof(Vertex))
{
 
   
    D3D11_BUFFER_DESC vbDesc;
    ZeroMemory(&vbDesc, sizeof(vbDesc));

    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.CPUAccessFlags = 0u;
    vbDesc.MiscFlags = 0u;
    vbDesc.ByteWidth = v.size()* sizeof(Vertex);
    vbDesc.StructureByteStride = stride;

    D3D11_SUBRESOURCE_DATA vbData = {};
    ZeroMemory(&vbData, sizeof(vbData));
    vbData.pSysMem = v.data();
    vbData.SysMemPitch = 0;
    vbData.SysMemSlicePitch = 0;
    hr = g.GetDevice()->CreateBuffer(&vbDesc, &vbData, &pVertexBuffer);
}

void VertexBuffer::Bind(Graphics& g)
{
    UINT offset = 0;
   g.GetContext()->IASetVertexBuffers(0,1,pVertexBuffer.GetAddressOf(),&stride, &offset);
}
