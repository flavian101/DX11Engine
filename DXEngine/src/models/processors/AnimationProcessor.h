#pragma once
#include <memory>
#include <vector>
#include <assimp/scene.h>


namespace DXEngine
{
	class AnimationClip;
	class Skeleton;
	struct Keyframe;

	class AnimationProcessor
	{
	public:
		AnimationProcessor() = default;

		std::vector<std::shared_ptr<AnimationClip>> ProcessAnimations(const aiScene* scene, std::shared_ptr<Skeleton> skeleton);
		std::shared_ptr<AnimationClip> ProcessAnimation(const aiAnimation* aiAnim, std::shared_ptr<Skeleton> skeleton);



		size_t GetAnimationsProcessed() const { return m_AnimationsProcessed; }
		void ResetStats() { m_AnimationsProcessed = 0; }


	private:
		std::vector<Keyframe> ProcessChannel(const aiNodeAnim* channel);

		// Keyframe interpolation helpers
		DirectX::XMFLOAT3 FindOrInterpolatePosition(const aiNodeAnim* channel, float time);
		DirectX::XMFLOAT4 FindOrInterpolateRotation(const aiNodeAnim* channel, float time);
		DirectX::XMFLOAT3 FindOrInterpolateScale(const aiNodeAnim* channel, float time);
		DirectX::XMFLOAT3 ConvertVector3(const aiVector3D& vector);
		DirectX::XMFLOAT4 ConvertQuaternion(const aiQuaternion& quat);



	private:
		size_t m_AnimationsProcessed = 0;
	};
}
