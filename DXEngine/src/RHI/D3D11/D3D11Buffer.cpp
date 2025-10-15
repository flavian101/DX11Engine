#include "dxpch.h"
#include "D3D11Buffer.h"


namespace DXEngine::RHI
{
	static D3D11_USAGE GetD3DUsage(BufferUsage usage)
	{
		switch (usage)
		{
		case BufferUsage::Static: return D3D11_USAGE_IMMUTABLE;
		case BufferUsage::Dynamic: return D3D11_USAGE_DYNAMIC;
		case BufferUsage::Stream:return D3D11_USAGE_DYNAMIC;
		default: return D3D11_USAGE_DEFAULT;
		}
	}

	static UINT GetBindFlags(BufferType type)
	{
		switch (type) {
		case BufferType::Vertex: return D3D11_BIND_VERTEX_BUFFER;
		case BufferType::Index: return D3D11_BIND_INDEX_BUFFER;
		case BufferType::Uniform: return D3D11_BIND_CONSTANT_BUFFER;
		case BufferType::Storage:
			return D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		default: return 0;
		}
	}

	static UINT GetCPUAccessFlags(BufferUsage usage) {
		if (usage == BufferUsage::Dynamic || usage == BufferUsage::Stream) {
			return D3D11_CPU_ACCESS_WRITE;
		}
		return 0;
	}


	D3D11Buffer::D3D11Buffer(ID3D11Device* device, const BufferDesc& desc)
		:m_Desc(desc)
	{
		m_Device = device;
		device->GetImmediateContext(m_Context.GetAddressOf());

		//16 bytes aligned if constant buffer
		uint32_t size = desc.size;
		if (desc.type == BufferType::Uniform)
		{
			size = (size + 15) & ~15;
		}

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = size;
		bufferDesc.Usage = GetD3DUsage(desc.usage);
		bufferDesc.BindFlags = GetBindFlags(desc.type);
		bufferDesc.CPUAccessFlags = GetCPUAccessFlags(desc.usage);
		bufferDesc.StructureByteStride = desc.stride;

		if (desc.type == BufferType::Storage)
		{
			bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		}

		D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
		D3D11_SUBRESOURCE_DATA initData = {};
		if (desc.initialData) {
			initData.pSysMem = desc.initialData;
			pInitData = &initData;
		}

		HRESULT hr = device->CreateBuffer(&bufferDesc, pInitData, m_Buffer.GetAddressOf());
		if (FAILED(hr)) {
			throw std::runtime_error("Failed to create D3D11 buffer");
		}

		if (!desc.debugName.empty()) {
			SetDebugName(desc.debugName);
		}

	}
	void D3D11Buffer::SetDebugName(const std::string& name)
	{
		m_DebugName = name;
		if (m_Buffer)
		{
			m_Buffer->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.length()), name.c_str());
		}
	}
	bool D3D11Buffer::Update(const void* data, uint32_t size, uint32_t offset)
	{
		if (!data || !m_Buffer)
		{
			OutputDebugStringA("D3D11Buffer: The buffer or data is null");
			return false;
		}

		if (m_Desc.usage == BufferUsage::Dynamic || m_Desc.usage == BufferUsage::Stream)
		{
			//map/umap for dynamic buffers
			D3D11_MAPPED_SUBRESOURCE mapped;
			HRESULT hr = m_Context->Map(m_Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			if (SUCCEEDED(hr)) {
				memcpy(static_cast<char*>(mapped.pData) + offset, data, size);
				m_Context->Unmap(m_Buffer.Get(), 0);
				return true;
			}
		}
		else
		{
			// UpdateSubresource for default buffers
			D3D11_BOX box = {};
			box.left = offset;
			box.right = offset + size;
			box.top = 0;
			box.bottom = 1;
			box.front = 0;
			box.back = 1;

			m_Context->UpdateSubresource(m_Buffer.Get(), 0, &box, data, 0, 0);
			return true;
		}

		return false;
	}
	bool D3D11Buffer::Read(void* outData, uint32_t size, uint32_t offset)
	{
		if (!m_Buffer || !outData || size == 0)
			return false;

		if (m_Desc.type != BufferType::Staging)
		{
			assert(false && "Can only read from staging buffers");
			return false;
		}
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT hr = m_Context->Map(m_Buffer.Get(), 0,
			D3D11_MAP_READ, 0, &mappedResource);
		if (FAILED(hr))
			return false;

		memcpy(outData, static_cast<const char*>(mappedResource.pData) + offset, size);
		m_Context->Unmap(m_Buffer.Get(), 0);
		return true;
	}
	void* D3D11Buffer::Map()
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = m_Context->Map(m_Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		return SUCCEEDED(hr) ? mapped.pData : nullptr;
	}
	void D3D11Buffer::UnMap()
	{
		m_Context->Unmap(m_Buffer.Get(), 0);
	}
}