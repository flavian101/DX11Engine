#include "dxpch.h"
#include "D3D11Buffer.h"


namespace DXEngine::RHI
{
	// The D3D usage a description turns into. A buffer the GPU writes (UAV) must be DEFAULT: D3D11 does not allow
	// UNORDERED_ACCESS on DYNAMIC or IMMUTABLE resources.
	static D3D11_USAGE GetD3DUsage(const BufferDesc& desc)
	{
		if (desc.type == BufferType::Storage && desc.allowUnorderedAccess)
			return D3D11_USAGE_DEFAULT;

		switch (desc.usage)
		{
		case BufferUsage::Static: return D3D11_USAGE_IMMUTABLE;
		case BufferUsage::Dynamic: return D3D11_USAGE_DYNAMIC;
		case BufferUsage::Stream:return D3D11_USAGE_DYNAMIC;
		default: return D3D11_USAGE_DEFAULT;
		}
	}

	static UINT GetBindFlags(const BufferDesc& desc)
	{
		switch (desc.type) {
		case BufferType::Vertex: return D3D11_BIND_VERTEX_BUFFER;
		case BufferType::Index: return D3D11_BIND_INDEX_BUFFER;
		case BufferType::Uniform: return D3D11_BIND_CONSTANT_BUFFER;
		case BufferType::Storage:
		{
			// Read-only unless the caller asked for GPU writes. (Asking for the UAV unconditionally made every
			// Storage buffer un-creatable as DYNAMIC, which is what a CPU-updated light buffer needs.)
			UINT flags = D3D11_BIND_SHADER_RESOURCE;
			if (desc.allowUnorderedAccess)
				flags |= D3D11_BIND_UNORDERED_ACCESS;
			return flags;
		}
		default: return 0;
		}
	}

	static UINT GetCPUAccessFlags(D3D11_USAGE usage) {
		return usage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0;
	}


	D3D11Buffer::D3D11Buffer(ID3D11Device* device, const BufferDesc& desc)
		:m_Desc(desc)
	{
		m_Device = device;
		device->GetImmediateContext(m_Context.GetAddressOf());

		// Rules every backend shares (structured-buffer stride, Static needs data, ...): fail with a sentence that
		// says what is wrong instead of a bare HRESULT.
		const std::string problem = ValidateBufferDesc(desc);
		if (!problem.empty())
		{
			OutputDebugStringA(("D3D11Buffer: " + problem + "\n").c_str());
			throw std::runtime_error("Invalid buffer description: " + problem);
		}

		//16 bytes aligned if constant buffer
		uint32_t size = desc.size;
		if (desc.type == BufferType::Uniform)
		{
			size = (size + 15) & ~15;
		}
		m_ByteWidth = size;
		m_D3DUsage  = GetD3DUsage(desc);

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = size;
		bufferDesc.Usage = m_D3DUsage;
		bufferDesc.BindFlags = GetBindFlags(desc);
		bufferDesc.CPUAccessFlags = GetCPUAccessFlags(m_D3DUsage);

		if (desc.type == BufferType::Storage)
		{
			bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			bufferDesc.StructureByteStride = desc.stride;
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

		// Storage buffers are read in shaders: create the view once, here (a view is cheap, and the command buffer
		// then only has to hand it to the pipeline).
		if (bufferDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;                 // structured buffers have no format
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = size / desc.stride;

			hr = device->CreateShaderResourceView(m_Buffer.Get(), &srvDesc, m_SRV.GetAddressOf());
			if (FAILED(hr)) {
				throw std::runtime_error("Failed to create the shader resource view of a D3D11 Storage buffer");
			}
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
		if (m_SRV)
		{
			const std::string viewName = name + ".SRV";   // shows up in RenderDoc / PIX next to the buffer
			m_SRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(viewName.length()), viewName.c_str());
		}
	}
	bool D3D11Buffer::Update(const void* data, uint32_t size, uint32_t offset)
	{
		if (!data || !m_Buffer)
		{
			OutputDebugStringA("D3D11Buffer: The buffer or data is null");
			return false;
		}

		// A write past the end would be a heap overflow on the Map path (memcpy) and an invalid box otherwise.
		if (offset > m_ByteWidth || size > m_ByteWidth - offset)
		{
			OutputDebugStringA("D3D11Buffer::Update: the range is outside the buffer\n");
			return false;
		}

		if (m_D3DUsage == D3D11_USAGE_IMMUTABLE)
		{
			OutputDebugStringA("D3D11Buffer::Update: a Static buffer cannot be updated\n");
			return false;
		}

		if (m_D3DUsage == D3D11_USAGE_DYNAMIC)
		{
			//map/umap for dynamic buffers. DISCARD: everything outside [offset, offset + size) is undefined afterwards.
			D3D11_MAPPED_SUBRESOURCE mapped;
			HRESULT hr = m_Context->Map(m_Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			if (SUCCEEDED(hr)) {
				memcpy(static_cast<char*>(mapped.pData) + offset, data, size);
				m_Context->Unmap(m_Buffer.Get(), 0);
				return true;
			}
			return false;
		}

		// UpdateSubresource for default buffers (including GPU-written Storage buffers)
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
		if (m_D3DUsage != D3D11_USAGE_DYNAMIC)
			return nullptr;   // only CPU-writable (dynamic / stream) buffers can be mapped

		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = m_Context->Map(m_Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		return SUCCEEDED(hr) ? mapped.pData : nullptr;
	}
	void D3D11Buffer::UnMap()
	{
		m_Context->Unmap(m_Buffer.Get(), 0);
	}

	void* D3D11Buffer::MapRange(uint32_t offset, uint32_t size)
	{
		if (!m_Buffer || !m_Context)
			return nullptr;

		// only CPU-writable (dynamic / stream) buffers can be mapped
		if (m_D3DUsage != D3D11_USAGE_DYNAMIC)
		{
			OutputDebugStringA("D3D11Buffer::MapRange: only dynamic buffers can be mapped\n");
			return nullptr;
		}

		if (size == 0 || offset > m_ByteWidth || size > m_ByteWidth - offset)
		{
			OutputDebugStringA("D3D11Buffer::MapRange: range is outside the buffer\n");
			return nullptr;
		}

		// D3D11 can only map a WHOLE buffer, so the "range" is a pointer offset into that mapping.
		// NO_OVERWRITE keeps the rest of the data intact (you promise not to touch what the GPU may still
		// read) but D3D11 allows it only on vertex/index buffers; other buffers (constant buffers) must use
		// DISCARD, which invalidates everything outside the range.
		const bool keepRest = m_Desc.type == BufferType::Vertex || m_Desc.type == BufferType::Index;

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		const HRESULT hr = m_Context->Map(m_Buffer.Get(), 0,
			keepRest ? D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (FAILED(hr))
			return nullptr;

		return static_cast<char*>(mapped.pData) + offset;
	}
}