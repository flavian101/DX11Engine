#pragma once
#include "utils/Mesh/Resource/MeshResource.h"

namespace DXEngine
{
	class InstancedMeshResource : public MeshResource
	{
	public:
		InstancedMeshResource(const std::string& name = "InstancedMesh") : MeshResource(name) {}

		// Instance data management
		void SetInstanceLayout(const VertexLayout& layout);
		const VertexLayout& GetInstanceLayout() const { return m_InstanceLayout; }

		void SetInstanceData(std::unique_ptr<VertexData> instanceData);
		const VertexData* GetInstanceData() const { return m_InstanceData.get(); }
		VertexData* GetInstanceData() { return m_InstanceData.get(); }

		size_t GetInstanceCount() const;

	private:
		VertexLayout m_InstanceLayout;
		std::unique_ptr<VertexData> m_InstanceData;
	};
}


