#pragma once
#include <DirectXMath.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "RHI/GraphicsTypes.h"

namespace DXEngine
{

	class VertexLayout
	{
	public:
		VertexLayout() = default;

		VertexLayout& AddAttribute(const RHI::VertexAttribute& attribute);
		VertexLayout& AddAttribute(RHI::VertexAttributeType type, RHI::DataFormat format, uint32_t slot = 0, bool perInstance = false);

		VertexLayout& Position(RHI::DataFormat format = RHI::DataFormat::Float3, uint32_t slot = 0);
		VertexLayout& Normal(RHI::DataFormat format = RHI::DataFormat::Float3, uint32_t slot = 0);
		VertexLayout& Tangent(RHI::DataFormat format = RHI::DataFormat::Float4, uint32_t slot = 0);
		VertexLayout& TexCoord(uint32_t index = 0, RHI::DataFormat format = RHI::DataFormat::Float2, uint32_t slot = 0);
		VertexLayout& Color(uint32_t index = 0, RHI::DataFormat format = RHI::DataFormat::Float4, uint32_t slot = 0);
		// Bone indices are stored as four 32-bit integers (UInt4 = DXGI R32G32B32A32_UINT, read as 'uint4' by the skinning shaders):
		// that is the size the importer writes (SetAttribute<XMINT4>, 16 bytes) and PostProcessor reads back. A narrower format
		// here (it used to be UByte4, 4 bytes) makes the BlendWeights write overwrite indices 1-3, so every vertex ends up with
		// one real bone and its other influences on bone 0.
		VertexLayout& BlendData(RHI::DataFormat indicesFormat = RHI::DataFormat::UInt4,
								RHI::DataFormat weightsFormat = RHI::DataFormat::Float4, uint32_t slot = 0);

		void Finalize();

		const std::vector<RHI::VertexAttribute>& GetAttributes() const { return m_Attributes; }
		uint32_t GetStride(uint32_t slot = 0) const;
		uint32_t GetAttributeCount() const { return static_cast<uint32_t>(m_Attributes.size()); }
		bool IsFinalized() const { return m_Finalized; }

		bool HasAttribute(RHI::VertexAttributeType type, uint32_t slot = 0) const;
		const RHI::VertexAttribute* FindAttribute(RHI::VertexAttributeType type, uint32_t slot = 0) const;

		std::string GetDebugString() const;

		static VertexLayout CreateBasic();          // Position + Normal + TexCoord
		static VertexLayout CreateLit();            // Position + Normal + Tangent + TexCoord
		static VertexLayout CreateUI();             // Position + TexCoord + Color
		static VertexLayout CreateSkinned();        // Position + Normal + Tangent + TexCoord + Blend data
		static VertexLayout CreateParticle();       // Position + Color + Size (for point sprites)



	private:
		void CalculateOffsetsAndStrides();
	private:
		std::vector<RHI::VertexAttribute> m_Attributes;
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

		// Moves every vertex to the place 'remap' gives it (old index -> new index, MeshOpt::kUnusedVertex = drop it) in
		// EVERY slot, so a vertex stays one vertex whatever streams it is spread over. 'newCount' is the number of vertices
		// that remain. The caller rewrites the indices with the same table.
		void Remap(const std::vector<uint32_t>& remap, size_t newCount);

		// Add vertex data
		template<typename T>
		void SetAttribute(size_t vertexIndex, RHI::VertexAttributeType type, const T& value, uint32_t slot = 0);

		template<typename T>
		T GetAttribute(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot = 0) const;

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
	void VertexData::SetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, RHI::VertexAttributeType type,
		const DirectX::XMFLOAT4& value, uint32_t slot);
	template<>
	void VertexData::SetAttribute<DirectX::XMFLOAT3>(size_t vertexIndex, RHI::VertexAttributeType type,
		const DirectX::XMFLOAT3& value, uint32_t slot);

	template<>
	void VertexData::SetAttribute<DirectX::XMFLOAT2>(size_t vertexIndex, RHI::VertexAttributeType type,
		const DirectX::XMFLOAT2& value, uint32_t slot);

	template<>
	void VertexData::SetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, RHI::VertexAttributeType type,
		const DirectX::XMFLOAT4& value, uint32_t slot);

	template<>
	void VertexData::SetAttribute<uint32_t[4]>(size_t vertexIndex, RHI::VertexAttributeType type,
		const uint32_t(&value)[4], uint32_t slot);

	template<>
	void VertexData::SetAttribute<DirectX::XMUINT4>(size_t vertexIndex, RHI::VertexAttributeType type,
		const DirectX::XMUINT4& value, uint32_t slot);

	template<>
	void VertexData::SetAttribute<DirectX::XMINT4>(size_t vertexIndex, RHI::VertexAttributeType type,
		const DirectX::XMINT4& value, uint32_t slot);

	template<>
	DirectX::XMFLOAT3 VertexData::GetAttribute<DirectX::XMFLOAT3>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const;

	template<>
	DirectX::XMFLOAT2 VertexData::GetAttribute<DirectX::XMFLOAT2>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const;

	template<>
	DirectX::XMFLOAT4 VertexData::GetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const;

	template<>
	DirectX::XMUINT4 VertexData::GetAttribute<DirectX::XMUINT4>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const;

	template<>
	DirectX::XMINT4 VertexData::GetAttribute<DirectX::XMINT4>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const;

}

