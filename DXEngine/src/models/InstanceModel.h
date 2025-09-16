#pragma once
#include "Model.h"

namespace DXEngine
{

	class InstanceModel :public Model
	{
	public:
		InstanceModel();
		explicit InstanceModel(std::shared_ptr<Mesh> mesh);

		void SetInstanceTransform(const std::vector<DirectX::XMFLOAT4X4>& transforms);
		void AddInstance(const DirectX::XMFLOAT4X4& transform);

		void ClearInstances();

		size_t GetInstanceCount()const { return m_InstanceTransforms.size(); }
		const std::vector<DirectX::XMFLOAT4X4>& GetInstanceTransforms() const { return m_InstanceTransforms; }

		BoundingBox GetWorldBoundingBox()const override;
		BoundingSphere GetWorldBoundingSphere()const override;

	private:
		std::vector<DirectX::XMFLOAT4X4> m_InstanceTransforms;

	};
}

