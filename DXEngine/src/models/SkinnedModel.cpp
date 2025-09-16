#include "dxpch.h"
#include "SkinnedModel.h"

namespace DXEngine
{
	//skinned Model implementation
	SkinnedModel::SkinnedModel()
		:Model()
	{
	}

	SkinnedModel::SkinnedModel(std::shared_ptr<Mesh> mesh)
		:Model(mesh)
	{
	}

	void SkinnedModel::SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& boneMatrices)
	{
		m_BoneMatrices = boneMatrices;
		//note: bone matrices might affect bounds, but typically we use pose bounds
		//for animated bounds, you'd need to transdorm vertices by bone matrices

	}

	void SkinnedModel::Update(float deltatime)
	{
		Model::Update();
		//this is where you'd update  bone matrices based on current animation state
	}

}