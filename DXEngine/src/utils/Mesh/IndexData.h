#pragma once
#include <variant>
#include "VertexAttribute.h"

namespace DXEngine
{
	enum class IndexType
	{
		UInt16,
		UInt32
	};

	class IndexData
	{
	public:
		IndexData(IndexType type = IndexType::UInt16) : m_IndexType(type) {}

		void SetIndexType(IndexType type);
		IndexType GetIndexType() const { return m_IndexType; }

		void Reserve(size_t count);
		void Clear();

		// Add indices
		void AddIndex(uint32_t index);
		void AddTriangle(uint32_t i0, uint32_t i1, uint32_t i2);
		void AddQuad(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3); // Two triangles

		// Bulk operations
		template<typename T>
		void SetIndices(const std::vector<T>& indices);

		void SetIndices(const std::vector<uint16_t>& indices);
		void SetIndices(const std::vector<uint32_t>& indices);

		// Access
		size_t GetIndexCount() const;
		size_t GetDataSize() const;
		const void* GetData() const;
		void* GetData();

		// Utility
		uint32_t GetIndex(size_t index) const;
		void SetIndex(size_t index, uint32_t value);

		// Optimization
		void OptimizeForCache();  // Vertex cache optimization
		void GenerateAdjacency(const VertexData& vertices, std::vector<uint32_t>& adjacency);

	private:
		IndexType m_IndexType;
		std::variant<std::vector<uint16_t>, std::vector<uint32_t>> m_Indices;
	};
}
