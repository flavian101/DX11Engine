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

		//add Skinned mesh
		virtual void AddMesh(std::shared_ptr<Mesh> mesh, const std::string& name = "skinnedSubmesh") override;

		// Set the skeleton for this model
		void SetSkeleton(std::shared_ptr<Skeleton> skeleton);
		const std::shared_ptr<Skeleton>& GetSkeleton() const { return m_Skeleton; }

		// Animation control
		void SetAnimationController(std::shared_ptr<AnimationController> controller);
		const std::shared_ptr<AnimationController>& GetAnimationController() const { return m_AnimationController; }

		//clip Managment 
		void AddAnimationClip(std::shared_ptr<AnimationClip> clip);
		void AddAnimationClip(const std::string& name, std::shared_ptr<AnimationClip> clip);
		std::shared_ptr<AnimationClip> GetAnimationClip(const std::string& name)const;
		std::shared_ptr<AnimationClip> GetAnimationClip(size_t index) const;
		size_t GetAnimationClipCount()const { return m_AnimationClips.size(); }
		const std::vector<std::shared_ptr<AnimationClip>>& GetAllAnimationClips() const { return m_AnimationClips; }
		std::vector<std::string> GetAnimationClipNames() const;


		// Quick animation access
		void PlayAnimation(std::shared_ptr<AnimationClip> clip, PlaybackMode mode = PlaybackMode::Loop);
		void PlayAnimation(const std::string& name, PlaybackMode mode = PlaybackMode::Loop);
		void PlayAnimation(size_t index, PlaybackMode mode = PlaybackMode::Loop);
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

		bool ValidateAnimation(std::shared_ptr<AnimationClip> clip) const;

		// Query states
		bool IsAnimating() const { return m_AnimationController && m_AnimationController->IsPlaying(); }
		float GetAnimationTime() const { return m_AnimationController ? m_AnimationController->GetCurrentTime() : 0.0f; }
		float GetAnimationNormalizedTime() const { return m_AnimationController ? m_AnimationController->GetNormalizedTime() : 0.0f; }

	private:
		void UpdateBoneMatrices();
		void InitializeBoneMatrices();

	private:
		std::shared_ptr<Skeleton> m_Skeleton;
		std::shared_ptr<AnimationController> m_AnimationController;
		std::vector<DirectX::XMFLOAT4X4> m_BoneMatrices;

		//Animation clip storage
		std::vector<std::shared_ptr<AnimationClip>> m_AnimationClips;
		std::unordered_map<std::string, size_t> m_AnimationClipNameMap;
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

		// Create with multiple animations 
		std::shared_ptr<SkinnedModel> CreateWithAnimations(
			std::shared_ptr<SkinnedMesh> mesh,
			std::shared_ptr<Skeleton> skeleton,
			const std::vector<std::shared_ptr<AnimationClip>>& clips,
			size_t defaultClipIndex = 0);
	}
}

