#pragma once
#include <DirectXMath.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace DXEngine
{
	class VertexLayout;

	enum class VertexAttributeType
	{
		Position,
		Normal,
		Tangent,
		Bitangent,
		TexCoord0,
		TexCoord1,
		TexCoord2,
		TexCoord3,
		Color0,
		Color1,
		BlendIndices,
		BlendWeights,
		Custom

	};
	enum class DataFormat // format specification
	{
		Float,
		Float2,
		Float3,
		Float4,
		Int,
		Int2,
		Int3,
		Int4,
		UByte4,
		UByte4N,    // Normalized
		Short2,
		Short2N,    // Normalized
		Short4,
		Short4N,    // Normalized
		Half2,
		Half4
	};

	struct VertexAttribute
	{
		VertexAttributeType Type;
		DataFormat Format;
		std::string SemanticName;
		uint32_t SemanticIndex;
		uint32_t Offset;
		uint32_t Slot; //mutiple buffers
		bool PerInstance;

		VertexAttribute() = default;
		VertexAttribute(VertexAttributeType attrType,
			DataFormat dataFormat,
			const std::string& semantic = "",
			uint32_t semIndex= 0,
			uint32_t inputSlot = 0,
			bool instanceData = false)
			: Type(attrType)
			, Format(dataFormat)
			, SemanticName(semantic.empty() ? GetDefaultSemanticName(attrType) : semantic)
			, SemanticIndex(semIndex)
			, Offset(0)  // Will be calculated by layout
			, Slot(inputSlot)
			, PerInstance(instanceData){ }
		
		uint32_t GetSize()const;
		DXGI_FORMAT GetDXGIFormat()const;
		static std::string GetDefaultSemanticName(VertexAttributeType type);
	};

	class VertexLayout
	{
	public:
		VertexLayout() = default;

		VertexLayout& AddAttribute(const VertexAttribute& attribute);
		VertexLayout& AddAttribute(VertexAttributeType type, DataFormat format, uint32_t slot = 0, bool perInstance = false);

		VertexLayout& Position(DataFormat format = DataFormat::Float3, uint32_t slot = 0);
		VertexLayout& Normal(DataFormat format = DataFormat::Float3, uint32_t slot = 0);
		VertexLayout& Tangent(DataFormat format = DataFormat::Float4, uint32_t slot = 0);
		VertexLayout& TexCoord(uint32_t index = 0, DataFormat format = DataFormat::Float2, uint32_t slot = 0);
		VertexLayout& Color(uint32_t index = 0, DataFormat format = DataFormat::Float4, uint32_t slot = 0);
		VertexLayout& BlendData(DataFormat indicesFormat = DataFormat::UByte4,
								DataFormat weightsFormat = DataFormat::Float4, uint32_t slot = 0);

		void Finalize();

		const std::vector<VertexAttribute>& GetAttributes() const { return m_Attributes; }
		uint32_t GetStride(uint32_t slot = 0) const;
		uint32_t GetAttributeCount() const { return static_cast<uint32_t>(m_Attributes.size()); }
		bool IsFinalized() const { return m_Finalized; }

		std::vector<D3D11_INPUT_ELEMENT_DESC> CreateD3D11InputElements() const;

		bool HasAttribute(VertexAttributeType type, uint32_t slot = 0) const;
		const VertexAttribute* FindAttribute(VertexAttributeType type, uint32_t slot = 0) const;

		std::string GetDebugString() const;

		static VertexLayout CreateBasic();          // Position + Normal + TexCoord
		static VertexLayout CreateLit();            // Position + Normal + Tangent + TexCoord
		static VertexLayout CreateUI();             // Position + TexCoord + Color
		static VertexLayout CreateSkinned();        // Position + Normal + Tangent + TexCoord + Blend data
		static VertexLayout CreateParticle();       // Position + Color + Size (for point sprites)



	private:
		void CalculateOffsetsAndStrides();
	private:
		std::vector<VertexAttribute> m_Attributes;
		std::unordered_map<uint32_t, uint32_t> m_SlotStrides;  // Stride per input slot
		bool m_Finalized = false;
	};

	class VertexData
	{
	public:
		VertexData(const VertexLayout& layout);
		~VertexData() = default;

		// Data manipulation
		void Reserve(size_t vertexCount);
		void Resize(size_t vertexCount);
		void Clear();

		// Add vertex data
		template<typename T>
		void SetAttribute(size_t vertexIndex, VertexAttributeType type, const T& value, uint32_t slot = 0);

		template<typename T>
		T GetAttribute(size_t vertexIndex, VertexAttributeType type, uint32_t slot = 0) const;

		// Bulk operations
		void* GetVertexData(uint32_t slot = 0) { return m_Data[slot].data(); }
		const void* GetVertexData(uint32_t slot = 0) const { return m_Data.at(slot).data(); }

		size_t GetVertexCount() const { return m_VertexCount; }
		size_t GetDataSize(uint32_t slot = 0) const { return m_Data.at(slot).size(); }

		const VertexLayout& GetLayout() const { return m_Layout; }

		// Validation
		bool IsValid() const;

	private:
		VertexLayout m_Layout;
		std::unordered_map<uint32_t, std::vector<uint8_t>> m_Data;  // Data per slot
		size_t m_VertexCount = 0;
	};

	// Template specializations for common types
	template<>
	void VertexData::SetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, VertexAttributeType type,
		const DirectX::XMFLOAT4& value, uint32_t slot);
	template<>
	void VertexData::SetAttribute<DirectX::XMFLOAT3>(size_t vertexIndex, VertexAttributeType type,
		const DirectX::XMFLOAT3& value, uint32_t slot);

	template<>
	void VertexData::SetAttribute<DirectX::XMFLOAT2>(size_t vertexIndex, VertexAttributeType type,
		const DirectX::XMFLOAT2& value, uint32_t slot);

	template<>
	void VertexData::SetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, VertexAttributeType type,
		const DirectX::XMFLOAT4& value, uint32_t slot);

	template<>
	void VertexData::SetAttribute<uint32_t[4]>(size_t vertexIndex, VertexAttributeType type,
		const uint32_t(&value)[4], uint32_t slot);

	template<>
	void VertexData::SetAttribute<DirectX::XMUINT4>(size_t vertexIndex, VertexAttributeType type,
		const DirectX::XMUINT4& value, uint32_t slot);

	template<>
	void VertexData::SetAttribute<DirectX::XMINT4>(size_t vertexIndex, VertexAttributeType type,
		const DirectX::XMINT4& value, uint32_t slot);

	template<>
	DirectX::XMFLOAT3 VertexData::GetAttribute<DirectX::XMFLOAT3>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const;

	template<>
	DirectX::XMFLOAT2 VertexData::GetAttribute<DirectX::XMFLOAT2>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const;

	template<>
	DirectX::XMFLOAT4 VertexData::GetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const;

	template<>
	DirectX::XMUINT4 VertexData::GetAttribute<DirectX::XMUINT4>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const;

}

