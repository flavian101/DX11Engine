#pragma once
#include "renderer/RendererCommand.h"
#include "ConstantBufferTypes.h" 
#include <wrl.h>

namespace DXEngine {

	enum class BufferType
	{
		Vertex,
		Index,
		Constant,
		Structured,
		Staging
	};

	enum class UsageType
	{
		Default,
		Dynamic,
		Immutable,
		Staging
	};

	struct BufferDesc
	{
		BufferType bufferType = BufferType::Vertex;
		UsageType usageType = UsageType::Default;
		UINT byteWidth = 0;
		UINT bindFlags = 0;
		UINT structureByteStride = 0;
		const void* initialData = nullptr;
		bool allowCPURead = false;
		bool allowUnorderedAccess = false;
	};

	class BufferBase
	{
	public:
		BufferBase() = default;
		virtual ~BufferBase() = default;

		//non-copyable
		BufferBase(const BufferBase&) = delete;
		BufferBase& operator=(const BufferBase&) = delete;
		//movable
		BufferBase(BufferBase&&) = default;
		BufferBase& operator=(BufferBase&&) = default;

		ID3D11Buffer* GetBuffer() const { return m_Buffer.Get(); }
		ID3D11Buffer* const* GetAddressOf() const { return m_Buffer.GetAddressOf(); }
		BufferType GetBufferType() const { return m_BufferType; }
		UsageType GetUsageType() const { return m_UsageType; }
		UINT GetByteWidth() const { return m_ByteWidth; }
		bool IsValid() const { return m_Buffer != nullptr; }

	protected:
		bool InitializeInternal(const BufferDesc& desc);
		bool UpdateInternal(const void* data, UINT dataSize, UINT offset = 0);
		bool ReadData(void* outData, UINT dataSize, UINT offset = 0)const;
		UINT CalculateAlignedSize(UINT size, BufferType type) const;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_Buffer;
		BufferType  m_BufferType = BufferType::Vertex;
		UsageType m_UsageType = UsageType::Default;
		UINT m_ByteWidth = 0;
		UINT m_StructuredbyteStride = 0;

	private:
		D3D11_USAGE GetD3DUsage(UsageType usage) const;
		UINT GetD3DBindFlags(BufferType type, bool allowUnorderedAccess) const;
		UINT GetD3DCPUAccessFlags(UsageType usage, bool allowCPURead) const;
	};
	
	//Typed Buffer
	template<class T>
	class TypedBuffer : public BufferBase
	{
	public:
		TypedBuffer() = default;

		bool Initialize(BufferType type, UsageType usage, const T* initialData = nullptr,
			bool allowUnorderedAccess = false)
		{
			BufferDesc desc;
			desc.bufferType = type;
			desc.usageType = usage;
			desc.byteWidth = CalculateAlignedSize(sizeof(T), type);
			desc.initialData = initialData;
			desc.allowUnorderedAccess = allowUnorderedAccess;
			desc.structureByteStride = (type == BufferType::Structured) ? sizeof(T) : 0;

			return InitializeInternal(desc);
		}

		bool InitializeArray(BufferType type, UsageType usage, const T* initialData,
			UINT elementCount, bool allowUnorderedAccess = false)
		{
			BufferDesc desc;
			desc.bufferType = type;
			desc.usageType = usage;
			desc.byteWidth = CalculateAlignedSize(sizeof(T) * elementCount, type);
			desc.initialData = initialData;
			desc.allowUnorderedAccess = allowUnorderedAccess;
			desc.structureByteStride = (type == BufferType::Structured) ? sizeof(T) : 0;

			return InitializeInternal(desc);
		}

		bool Update(const T& data)
		{
			return UpdateInternal(&data, sizeof(T));
		}

		//update Array
		bool UpdateArray(const T* data, UINT elementCount, UINT startElement = 0)
		{
			UINT offset = startElement * sizeof(T);
			UINT size = elementCount * sizeof(T);
			return UpdateInternal(data, size, offset);
		}

		//read data-block (for staging Buffer)
		bool ReadBlock(T* outData) const
		{
			return ReadData(&outData, sizeof(T));
		}

		bool ReadBackArray(T* outData, UINT elementCount, UINT startElement = 0) const
		{
			UINT offset = startElement * sizeof(T);
			UINT size = elementCount * sizeof(T);
			return ReadData(outData, size, offset);
		}

		UINT GetElementCount() const
		{
			return m_ByteWidth / sizeof(T);
		}
	};

	//specialized buffers 
	template<typename T>
	class VertexBuffer : public TypedBuffer<T>
	{
	public:
		bool Initialize(const T* vertices, UINT vertexCount, UsageType usage = UsageType::Immutable)
		{
			return this->InitializeArray(BufferType::Vertex, usage, vertices, vertexCount);
		}

		UINT GetStride() const { return sizeof(T); }
		UINT GetVertexCount() const { return this->GetElementCount(); }

	};

	template<typename T = UINT>
	class IndexBuffer : public TypedBuffer<T>
	{
		static_assert(std::is_same_v<T, UINT> || std::is_same_v<T, USHORT>,
			"Index buffer must use UINT or USHORT");
	public:
		bool Initialize(const T* indices, UINT indexCount, UsageType usage = UsageType::Immutable)
		{
			return this->InitializeArray(BufferType::Index, usage, indices, indexCount);
		}

		DXGI_FORMAT GetFormat() const
		{
			return std::is_same_v<T, UINT> ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
		}

		UINT GetIndexCount() const { return this->GetElementCount(); }
	};

	template<typename T>
	class ConstantBuffer : public TypedBuffer<T>
	{
	public:
		bool Initialize(const T* initialData = nullptr, UsageType usage = UsageType::Dynamic)
		{
			return TypedBuffer<T>::Initialize(BufferType::Constant, usage, initialData);
		}

		bool Update(const T& data)
		{
			return TypedBuffer<T>::Update(data);
		}
	};

	template<typename T>
	class StructuredBuffer : public TypedBuffer<T>
	{
	public:
		bool Initialize(const T* initialData, UINT elementCount, UsageType usage = UsageType::Default,
			bool allowUnorderedAccess = false)
		{
			return this->InitializeArray(BufferType::Structured, usage, initialData,
				elementCount, allowUnorderedAccess);
		}
	};

	// Raw buffer for untyped data
	class RawBuffer : public BufferBase
	{
	public:
		bool Initialize(const BufferDesc& desc)
		{
			return InitializeInternal(desc);
		}

		bool Update(const void* data, UINT dataSize, UINT offset = 0)
		{
			return UpdateInternal(data, dataSize, offset);
		}

		bool ReadBack(void* outData, UINT dataSize, UINT offset = 0) const
		{
			return ReadData(outData, dataSize, offset);
		}
	};
}

