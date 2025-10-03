#include "dxpch.h"
#include "SkinnedModel.h"
#include "utils/Mesh/SkinnedMesh.h"


namespace DXEngine
{
	//skinned Model implementation
	SkinnedModel::SkinnedModel()
		:Model()
	{
	}

	SkinnedModel::SkinnedModel(std::shared_ptr<SkinnedMesh> mesh)
		:Model(mesh)
	{
	}

	void SkinnedModel::SetSkeleton(std::shared_ptr<Skeleton> skeleton)
	{
		m_Skeleton = skeleton;
		if (m_Skeleton)
		{
			m_BoneMatrices.resize(m_Skeleton->GetBoneCount());
			for (auto& mat : m_BoneMatrices)
			{
				DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixIdentity());
			}
		}
	}

	void SkinnedModel::SetAnimationController(std::shared_ptr<AnimationController> controller)
	{
		m_AnimationController = controller;
	}

	void SkinnedModel::PlayAnimation(std::shared_ptr<AnimationClip> clip, PlaybackMode mode)
	{
		if (!m_AnimationController && m_Skeleton)
		{
			m_AnimationController = std::make_shared<AnimationController>(m_Skeleton);
		}

		if (m_AnimationController)
		{
			m_AnimationController->SetClip(clip);
			m_AnimationController->SetPlaybackMode(mode);
			m_AnimationController->Play();
		}
	}

	void SkinnedModel::StopAnimation()
	{
		if (m_AnimationController)
		{
			m_AnimationController->Stop();
		}
	}

	void SkinnedModel::PauseAnimation()
	{
		if (m_AnimationController)
		{
			m_AnimationController->Pause();
		}
	}

	void SkinnedModel::ResumeAnimation()
	{
		if (m_AnimationController)
		{
			m_AnimationController->Play();
		}
	}

	void SkinnedModel::SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& boneMatrices)
	{
		m_BoneMatrices = boneMatrices;
		// Update the skinned mesh if available
		auto skinnedMesh = std::dynamic_pointer_cast<SkinnedMesh>(GetMesh());
		if (skinnedMesh)
		{
			skinnedMesh->SetBoneMatrices(m_BoneMatrices);
		}

	}

	void SkinnedModel::Update(float deltaTime)
	{
		Model::Update(deltaTime); 

		// Update animation
		if (m_AnimationController)
		{
			m_AnimationController->Update(deltaTime);
			UpdateBoneMatrices();
		}
	}

	void SkinnedModel::BindBoneData() const
	{
		auto skinnedMesh = std::dynamic_pointer_cast<SkinnedMesh>(GetMesh());
		if (skinnedMesh)
		{
			skinnedMesh->BindBoneData();
		}
	}

	void SkinnedModel::UpdateBoneMatrices()
	{
		if (!m_AnimationController)
			return;

		// Get updated bone matrices from controller
		const auto& matrices = m_AnimationController->GetBoneMatrices();
		SetBoneMatrices(matrices);
	}

	namespace SkinnedModelFactory
	{
		std::shared_ptr<SkinnedModel> Create(
			std::shared_ptr<SkinnedMesh> mesh,
			std::shared_ptr<Skeleton> skeleton)
		{
			auto model = std::make_shared<SkinnedModel>(mesh);
			model->SetSkeleton(skeleton);
			return model;
		}

		std::shared_ptr<SkinnedModel> CreateWithAnimation(
			std::shared_ptr<SkinnedMesh> mesh,
			std::shared_ptr<Skeleton> skeleton,
			std::shared_ptr<AnimationClip> defaultClip)
		{
			auto model = Create(mesh, skeleton);

			auto controller = std::make_shared<AnimationController>(skeleton);
			controller->SetClip(defaultClip);
			model->SetAnimationController(controller);

			return model;
		}
	}

}

