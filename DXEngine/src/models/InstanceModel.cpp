#include "dxpch.h"
#include "InstanceModel.h"

namespace DXEngine
{
	InstanceModel::InstanceModel()
		:Model()
	{
	}

	InstanceModel::InstanceModel(std::shared_ptr<Mesh> mesh)
		:Model(mesh)
	{
	}

	void InstanceModel::SetInstanceTransform(const std::vector<DirectX::XMFLOAT4X4>& transforms)
	{
		m_InstanceTransforms = transforms;
		Model::InvalidateBounds();
	}

	void InstanceModel::AddInstance(const DirectX::XMFLOAT4X4& transform)
	{
		m_InstanceTransforms.push_back(transform);
		Model::InvalidateBounds();
	}

	void InstanceModel::ClearInstances()
	{
		m_InstanceTransforms.clear();
		Model::InvalidateBounds();
	}

	BoundingBox InstanceModel::GetWorldBoundingBox()const
	{
		if (m_InstanceTransforms.empty())
			return Model::GetWorldBoundingBox();

		BoundingBox localBox = GetLocalBoundingBox();
		BoundingBox combinedBox;
		bool first = true;

		for (const auto& instanceTransform : m_InstanceTransforms)
		{
			DirectX::XMMATRIX instanceMatrix = DirectX::XMLoadFloat4x4(&instanceTransform);
			DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixMultiply(GetModelMatrix(), instanceMatrix);

			DirectX::XMVECTOR corners[8];
			localBox.GetCorners(corners);

			for (int i = 0; i < 8; i++)
			{
				DirectX::XMVECTOR worldCorner = DirectX::XMVector3Transform(corners[i], worldMatrix);
				DirectX::XMFLOAT3 cornerPos;
				DirectX::XMStoreFloat3(&cornerPos, worldCorner);

				if (first)
				{
					combinedBox = BoundingBox(cornerPos, cornerPos);
					first = false;
				}
				else
				{
					combinedBox.Expand(cornerPos);
				}
			}
		}
		return combinedBox;
	}

	BoundingSphere InstanceModel::GetWorldBoundingSphere()const
	{
		if (m_InstanceTransforms.empty())
			return Model::GetWorldBoundingSphere();

		BoundingSphere localSphere = GetLocalBoundingSphere();

		DirectX::XMFLOAT3 combinedCenter(0.0f, 0.0f, 0.0f);
		float maxRadius = 0.0f;

		//find center of all instance positions
		for (const auto& instanceTransform : m_InstanceTransforms)
		{
			DirectX::XMMATRIX instanceMatrix = DirectX::XMLoadFloat4x4(&instanceTransform);
			DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixMultiply(GetModelMatrix(), instanceMatrix);

			DirectX::XMVECTOR centerVec = DirectX::XMLoadFloat3(&localSphere.center);
			DirectX::XMVECTOR worldCenter = DirectX::XMVector3Transform(centerVec, worldMatrix);

			DirectX::XMFLOAT3 worldCenterFloat;
			DirectX::XMStoreFloat3(&worldCenterFloat, worldCenter);

			combinedCenter.x += worldCenterFloat.x;
			combinedCenter.y += worldCenterFloat.y;
			combinedCenter.z += worldCenterFloat.z;
		}

		//average the center positions 
		float instanceCount = static_cast<float>(m_InstanceTransforms.size());

		combinedCenter.x /= instanceCount;
		combinedCenter.y /= instanceCount;
		combinedCenter.z /= instanceCount;

		//find the minimum distance from center to any instance
		for (const auto& instanceTransform : m_InstanceTransforms)
		{
			DirectX::XMMATRIX instanceMatrix = DirectX::XMLoadFloat4x4(&instanceTransform);
			DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixMultiply(GetModelMatrix(), instanceMatrix);

			DirectX::XMVECTOR centerVec = DirectX::XMLoadFloat3(&localSphere.center);
			DirectX::XMVECTOR worldCenter = DirectX::XMVector3Transform(centerVec, worldMatrix);

			DirectX::XMFLOAT3 worldCenterFloat;
			DirectX::XMStoreFloat3(&worldCenterFloat, worldCenter);


			///calculate scale factor for this instance
			DirectX::XMVECTOR scale, rotation, translation;
			DirectX::XMMatrixDecompose(&scale, &rotation, &translation, worldMatrix);
			float maxScale = DirectX::XMVectorGetX(DirectX::XMVectorMax(DirectX::XMVectorMax(
				DirectX::XMVectorSplatX(scale), DirectX::XMVectorSplatY(scale)), DirectX::XMVectorSplatZ(scale)));

			float scaledRadius = localSphere.radius * maxScale;

			//distance from combined Center to this instance center + radius
			float dx = worldCenterFloat.x - combinedCenter.x;
			float dy = worldCenterFloat.y - combinedCenter.y;
			float dz = worldCenterFloat.z - combinedCenter.z;

			float distance = DirectX::XMVectorGetX(
				DirectX::XMVector3Length(DirectX::XMVectorSet(dx, dy, dz, 0.0f))
			);
			maxRadius = std::max(maxRadius, distance);
		}

		return BoundingSphere(combinedCenter, maxRadius);
	}

}