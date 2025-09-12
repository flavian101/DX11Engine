#include "dxpch.h"
#include "Buffer.h"
#include <cassert>

namespace DXEngine
{
	bool BufferBase::InitializeInternal(const BufferDesc& desc)
	{
		if (desc.byteWidth == 0)
		{
			assert(false && "Buffer byte width cannot be zero");
			return false;
		}
		m_BufferType = desc.bufferType;
		m_UsageType = desc.usageType;
		m_ByteWidth = desc.byteWidth;
		m_StructuredbyteStride = desc.structureByteStride;

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = m_ByteWidth;
		bufferDesc.Usage = GetD3DUsage(desc.usageType);
		bufferDesc.BindFlags = GetD3DBindFlags(desc.bufferType, desc.allowUnorderedAccess);
		bufferDesc.CPUAccessFlags = GetD3DCPUAccessFlags(desc.usageType, desc.allowCPURead);
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = desc.structureByteStride;

		if (desc.bufferType == BufferType::Structured)
		{
			bufferDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		}

		if (desc.allowUnorderedAccess)
		{
			bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}

		D3D11_SUBRESOURCE_DATA* pInitialData = nullptr;
		D3D11_SUBRESOURCE_DATA initialData = {};
		if (desc.initialData)
		{
			initialData.pSysMem = desc.initialData;
			pInitialData = &initialData;
		}
			
		HRESULT hr = RenderCommand::GetDevice()->CreateBuffer(&bufferDesc, pInitialData, m_Buffer.GetAddressOf());
		return SUCCEEDED(hr);
	}
	bool BufferBase::UpdateInternal(const void* data, UINT dataSize, UINT offset)
	{
		if (!m_Buffer || !data || dataSize == 0)
			return false;

		if (m_UsageType == UsageType::Immutable)
		{
			assert(false && "Cannot update immutable buffer");
			return false;
		}
		
		if (m_UsageType == UsageType::Dynamic)
		{
			//use Map/Unmap for dynamic buffers
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			HRESULT hr = RenderCommand::GetContext()->Map(m_Buffer.Get(), 0, 
				D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			if (FAILED(hr))
				return false;

			memcpy(static_cast<char*>(mappedResource.pData) + offset, data, dataSize);
			RenderCommand::GetContext()->Unmap(m_Buffer.Get(), 0);
			return true;
		}
		else
		{
			// Use UpdateSubresource for default buffers
			D3D11_BOX box = {};
			box.left = offset;
			box.right = offset + dataSize;
			box.top = 0;
			box.bottom = 1;
			box.front = 0;
			box.back = 1;

			RenderCommand::GetContext()->UpdateSubresource(m_Buffer.Get(), 0, &box, data, 0, 0);
		}
		return false;
	}
	bool BufferBase::ReadData(void* outData, UINT dataSize, UINT offset) const
	{
		if (!m_Buffer || !outData || dataSize == 0)
			return false;

		if (m_UsageType != UsageType::Staging)
		{
			assert(false && "Can only read from staging buffers");
			return false;
		}
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT hr = RenderCommand::GetContext()->Map(m_Buffer.Get(), 0,
			D3D11_MAP_READ, 0, &mappedResource);
		if (FAILED(hr))
			return false;

		memcpy(outData, static_cast<const char*>(mappedResource.pData) + offset, dataSize);
		RenderCommand::GetContext()->Unmap(m_Buffer.Get(), 0);
		return true;
	}
	D3D11_USAGE BufferBase::GetD3DUsage(UsageType usage) const
	{
		switch (usage)
		{
		case UsageType::Default: return D3D11_USAGE_DEFAULT;
		case UsageType::Dynamic: return D3D11_USAGE_DYNAMIC;
		case UsageType::Immutable: return D3D11_USAGE_IMMUTABLE;
		case UsageType::Staging: return D3D11_USAGE_STAGING;
		default: return D3D11_USAGE_DEFAULT;
		}
	}
	UINT BufferBase::GetD3DBindFlags(BufferType type, bool allowUnorderedAccess) const
	{
		UINT bindFlags = 0;
		switch (type)
		{
		case BufferType::Vertex:
			bindFlags = D3D11_BIND_VERTEX_BUFFER;
			break;
		case BufferType::Index:
			bindFlags = D3D11_BIND_INDEX_BUFFER;
			break;
		case BufferType::Constant:
			bindFlags = D3D11_BIND_CONSTANT_BUFFER;
			break;
		case BufferType::Structured:
			bindFlags = D3D11_BIND_SHADER_RESOURCE;
			break;
		case BufferType::Staging:
			bindFlags = 0;
			break;

			if(allowUnorderedAccess && type != BufferType::Staging)
				bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		return bindFlags;
	}
	UINT BufferBase::GetD3DCPUAccessFlags(UsageType usage, bool allowCPURead) const
	{
		switch (usage)
		{
		case UsageType::Dynamic:
			return D3D11_CPU_ACCESS_WRITE;
		case UsageType::Staging:
			return allowCPURead ? (D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE) : D3D11_CPU_ACCESS_WRITE;
		default:
			return D3D11_USAGE_DEFAULT;
		}
	}
	UINT BufferBase::CalculateAlignedSize(UINT size, BufferType type) const
	{
		if (type == BufferType::Constant)
		{
			return (size + 15) & ~15;
		}
		return size;
	}
}