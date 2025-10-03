#pragma once
#include "Model.h"
#include "Animation/AnimationController.h"


namespace DXEngine
{
	class SkinnedMesh;

	class SkinnedModel : public Model
	{
	public:
		SkinnedModel();
		explicit SkinnedModel(std::shared_ptr<SkinnedMesh> mesh);

		// Set the skeleton for this model
		void SetSkeleton(std::shared_ptr<Skeleton> skeleton);
		const std::shared_ptr<Skeleton>& GetSkeleton() const { return m_Skeleton; }

		// Animation control
		void SetAnimationController(std::shared_ptr<AnimationController> controller);
		const std::shared_ptr<AnimationController>& GetAnimationController() const { return m_AnimationController; }

		// Quick animation access
		void PlayAnimation(std::shared_ptr<AnimationClip> clip, PlaybackMode mode = PlaybackMode::Loop);
		void StopAnimation();
		void PauseAnimation();
		void ResumeAnimation();

		// Bone matrix access
		void SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& boneMatrices);
		const std::vector<DirectX::XMFLOAT4X4>& GetBoneMatrices() const { return m_BoneMatrices; }

		// Override update to handle animation
		void Update(float deltaTime) override;

		// Rendering helper
		void BindBoneData() const;

	private:
		void UpdateBoneMatrices();

	private:
		std::shared_ptr<Skeleton> m_Skeleton;
		std::shared_ptr<AnimationController> m_AnimationController;
		std::vector<DirectX::XMFLOAT4X4> m_BoneMatrices;
	};

	namespace SkinnedModelFactory
	{
		// Create a skinned model from a mesh and skeleton
		std::shared_ptr<SkinnedModel> Create(
			std::shared_ptr<SkinnedMesh> mesh,
			std::shared_ptr<Skeleton> skeleton);

		// Create with animation controller
		std::shared_ptr<SkinnedModel> CreateWithAnimation(
			std::shared_ptr<SkinnedMesh> mesh,
			std::shared_ptr<Skeleton> skeleton,
			std::shared_ptr<AnimationClip> defaultClip);
	}
}

