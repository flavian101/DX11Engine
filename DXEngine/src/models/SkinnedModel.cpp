#include "dxpch.h"
#include "SkinnedModel.h"
#include "utils/Mesh/Resource/SkinnedMeshResource.h"
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
		if (mesh)
		{
			auto skinnedResource = mesh->GetSkinnedResource();
			if (skinnedResource)
			{
				m_Skeleton = skinnedResource->GetSkeleton();
				InitializeBoneMatrices();
			}
		}
	}

	void SkinnedModel::AddMesh(std::shared_ptr<Mesh> mesh, const std::string& name)
	{
		Model::AddMesh(mesh, name);

		//if it's a skinned mesh, ensure it has the same Skeleton
		auto skinnedMesh = std::dynamic_pointer_cast<SkinnedMesh>(mesh);
		if (skinnedMesh && m_Skeleton)
		{
			auto skinnedResource = skinnedMesh->GetSkinnedResource();
			if (skinnedResource && !skinnedResource->GetSkeleton())
			{
				skinnedResource->SetSkeleton(m_Skeleton);
			}

			// Set current bone matrices
			if (!m_BoneMatrices.empty())
			{
				skinnedMesh->SetBoneMatrices(m_BoneMatrices);
			}
		}
	}

	void SkinnedModel::SetSkeleton(std::shared_ptr<Skeleton> skeleton)
	{
		m_Skeleton = skeleton;

		//upadate the skeleton in the mesh Resource If we have a skeleton
		auto skinnedMesh = std::dynamic_pointer_cast<SkinnedMesh>(GetMesh());
		if (skinnedMesh)
		{
			auto skinnedResource = skinnedMesh->GetSkinnedResource();
			if (skinnedResource)
			{
				skinnedResource->SetSkeleton(skeleton);
			}
		}
		//InitializeBoneMatrices();
		
	}

	void SkinnedModel::SetAnimationController(std::shared_ptr<AnimationController> controller)
	{
		m_AnimationController = controller;

		//Ensure The controller has the same Skeleton
		if (m_AnimationController && m_Skeleton)
		{
			if (m_AnimationController->GetSkeleton() != m_Skeleton)
			{
				OutputDebugStringA("Warning: Animation controller skeleton doesn't match model skeleton\n");
			}
		}

	}

	void SkinnedModel::AddAnimationClip(std::shared_ptr<AnimationClip> clip)
	{
		if (!clip)
		{
			return;
		}
		AddAnimationClip(clip->GetName(), clip);
	}

	void SkinnedModel::AddAnimationClip(const std::string& name, std::shared_ptr<AnimationClip> clip)
	{
		if (!clip)
			return;

		//check if with this name already exists
		auto it = m_AnimationClipNameMap.find(name);
		if (it != m_AnimationClipNameMap.end())
		{
			//replace existing clip
			m_AnimationClips[it->second] = clip;
			OutputDebugStringA(("Warning: Replacing existing animation clip: " + name + "\n").c_str());
		}
		else
		{
			//add new clip
			size_t index = m_AnimationClips.size();
			m_AnimationClips.push_back(clip);
			m_AnimationClipNameMap[name] = index;
		}
 	}

	std::shared_ptr<AnimationClip> SkinnedModel::GetAnimationClip(const std::string& name) const
	{
		auto it = m_AnimationClipNameMap.find(name);
		if (it != m_AnimationClipNameMap.end())
		{
			return m_AnimationClips[it->second];
		}

		return nullptr;
	}

	std::shared_ptr<AnimationClip> SkinnedModel::GetAnimationClip(size_t index) const
	{
		if (index >= m_AnimationClips.size())
		{
			return nullptr;
		}
		return m_AnimationClips[index];
	}

	std::vector<std::string> SkinnedModel::GetAnimationClipNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_AnimationClips.size());
		
		for (const auto& clip : m_AnimationClips)
		{
			if (clip)
			{
				names.push_back(clip->GetName());
			}
		}
		return names;
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

	void SkinnedModel::PlayAnimation(const std::string& name, PlaybackMode mode)
	{
		auto clip = GetAnimationClip(name);
		if (!clip)
		{
			OutputDebugStringA(("Warning: Animation clip not found: " + name + "\n").c_str());
			return;
		}

		PlayAnimation(clip, mode);
	}

	void SkinnedModel::PlayAnimation(size_t index, PlaybackMode mode)
	{
		auto clip = GetAnimationClip(index);
		if (!clip)
		{
			OutputDebugStringA(("Warning: Animation clip index out of range: " +
				std::to_string(index) + "\n").c_str());
			return;
		}

		PlayAnimation(clip, mode);
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

	bool SkinnedModel::ValidateAnimation(std::shared_ptr<AnimationClip> clip) const
	{
		if (!clip || !m_Skeleton)
			return false;

		// Check if all bones in the animation exist in the skeleton
		for (const auto& boneAnim : clip->GetBoneAnimations())
		{
			if (m_Skeleton->GetBoneIndex(boneAnim.first) < 0)
			{
				OutputDebugStringA(("Warning: Animation contains unknown bone: " + boneAnim.first + "\n").c_str());
				return false;
			}
		}

		return true;
	}

	void SkinnedModel::UpdateBoneMatrices()
	{
		if (!m_AnimationController)
			return;

		// Get updated bone matrices from controller
		const auto& matrices = m_AnimationController->GetBoneMatrices();
		SetBoneMatrices(matrices);
	}

	void SkinnedModel::InitializeBoneMatrices()
	{
		if (m_Skeleton)
		{
			m_BoneMatrices.resize(m_Skeleton->GetBoneCount());
			for (auto& mat : m_BoneMatrices)
			{
				DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixIdentity());
			}
		}
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
		std::shared_ptr<SkinnedModel> CreateWithAnimations(std::shared_ptr<SkinnedMesh> mesh, std::shared_ptr<Skeleton> skeleton, const std::vector<std::shared_ptr<AnimationClip>>& clips, size_t defaultClipIndex)
		{
			auto model = Create(mesh, skeleton);

			// Add all animation clips
			for (const auto& clip : clips)
			{
				if (clip)
				{
					model->AddAnimationClip(clip);
				}
			}

			// Set up controller with default clip
			if (defaultClipIndex < clips.size() && clips[defaultClipIndex])
			{
				auto controller = std::make_shared<AnimationController>(skeleton);
				controller->SetClip(clips[defaultClipIndex]);
				model->SetAnimationController(controller);
			}

			return model;
		}
	}

}

