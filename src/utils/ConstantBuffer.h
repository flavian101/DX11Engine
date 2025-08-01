#pragma once
#include "Graphics.h"
#include "ConstantBufferTypes.h" 
#include <wrl.h>


template<class T>
class ConstantBuffer
{
private:
	ConstantBuffer(Graphics& g, const ConstantBuffer<T>& rhs);
	//HRESULT = hr;
	
public:
	ConstantBuffer(){}

	T data;
	ID3D11Buffer* Get()const
	{
		return buffer.Get();
	}
	ID3D11Buffer* const* GetAddressOf()const
	{
		return buffer.GetAddressOf();
	}

	void Initialize(Graphics& g)
	{
		D3D11_BUFFER_DESC desc;

		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.ByteWidth = static_cast<UINT>(sizeof(T) + (16 - (sizeof(T) % 16)));
		desc.StructureByteStride = 0;

		g.GetDevice()->CreateBuffer(&desc, 0, buffer.GetAddressOf());

	}
	
	bool Update(Graphics& g)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;

		
		 g.GetContext()->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
			0, &mappedResource);
		
		CopyMemory(mappedResource.pData, &data, sizeof(T));
		g.GetContext()->Unmap(buffer.Get(), 0);
		return true;
	}
	


private:
	
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
};


