#pragma once
#include "AnimationClip.h"
#include <DirectXMath.h>
#include <vector>

namespace DXEngine
{
	namespace AnimationMath
	{
		inline DirectX::XMFLOAT3 LerpVector(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t)
		{
			DirectX::XMVECTOR va = DirectX::XMLoadFloat3(&a);
			DirectX::XMVECTOR vb = DirectX::XMLoadFloat3(&b);
			DirectX::XMVECTOR result = DirectX::XMVectorLerp(va, vb, t);

			DirectX::XMFLOAT3 output;
			DirectX::XMStoreFloat3(&output, result);
			return output;
		}
		
		inline DirectX::XMFLOAT4 SlerpQuaternion(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float t)
		{
			DirectX::XMVECTOR qa = DirectX::XMLoadFloat4(&a);
			DirectX::XMVECTOR qb = DirectX::XMLoadFloat4(&b);
			DirectX::XMVECTOR result = DirectX::XMQuaternionSlerp(qa, qb, t);
			DirectX::XMFLOAT4 output;
			DirectX::XMStoreFloat4(&output, result);

			return output;
		}

		inline Keyframe InterpolateKeyframes(const Keyframe& k0, const Keyframe& k1, float t)
		{
			Keyframe result;
			result.TimeStamp = k0.TimeStamp + (k1.TimeStamp - k0.TimeStamp) * t;
			result.Translation = LerpVector(k0.Translation, k1.Translation, t);
			result.Rotation = SlerpQuaternion(k0.Rotation, k1.Rotation, t);
			result.Scale = LerpVector(k0.Scale, k1.Scale, t);
			return result;
		}

	}

	class AnimationEvaluator
	{
	public:
		AnimationEvaluator() = default;

		//evaluate animation at specific time and compute bone matrices
		void Evaluate(const AnimationClip& clip,
			const Skeleton& skeleton,
			float time,
			std::vector<DirectX::XMFLOAT4X4>& outBoneMatrices)
		{
			//Ensure output has correct size
			size_t boneCount = skeleton.GetBoneCount();
			outBoneMatrices.resize(boneCount);

			//Loop animation Time
			float animTime = fmod(time * clip.GetTicksPerSecond(), clip.GetDuration());

			//calculate local transforms for each bone
			std::vector<DirectX::XMMATRIX> localTransforms(boneCount);
	
			for (size_t i = 0; i < boneCount; i++)
			{
				const Bone& bone = skeleton.GetBone(i);
				const BoneAnimation* boneAnim = clip.GetBoneAnimation(bone.Name);

				if (boneAnim && !boneAnim->Keyframes.empty())
				{
					localTransforms[i] = EvaluteBoneAnimation(*boneAnim, animTime);
				}
				else
				{
					localTransforms[i] = DirectX::XMLoadFloat4x4(&bone.LocalTransform);
				}
			}

			//calculate world Transform (accumulate parent transform)
			std::vector<DirectX::XMMATRIX> worldTransforms(boneCount);
			CalculateWorldTransforms(skeleton, localTransforms, worldTransforms);

			// Calculate final bone matrices
			// Formula: FinalMatrix = OffsetMatrix * WorldTransform
			// - OffsetMatrix: transforms FROM mesh space TO bone's local space (bind pose)
			// - WorldTransform: animated transformation of the bone in world space
			for (size_t i = 0; i < boneCount; i++)
			{
				const Bone& bone = skeleton.GetBone(i);
				DirectX::XMMATRIX offsetMatrix = DirectX::XMLoadFloat4x4(&bone.OffsetMatrix);

				// This gives us: MeshSpace -> BoneLocalSpace -> AnimatedWorldSpace
				DirectX::XMMATRIX finalTransform = DirectX::XMMatrixMultiply(offsetMatrix, worldTransforms[i]);

				// *** TRANSPOSE for HLSL (HLSL uses column-major by default) ***
				DirectX::XMStoreFloat4x4(&outBoneMatrices[i], DirectX::XMMatrixTranspose(finalTransform));
			}
		}

	private:
		//Evaluate Animation for a single Bone
		DirectX::XMMATRIX EvaluteBoneAnimation(const BoneAnimation& boneAnim, float time)
		{
			size_t frame0, frame1;
			float interpolation;
			boneAnim.GetFrameIndices(time, frame0, frame1, interpolation);

			const Keyframe& k0 = boneAnim.Keyframes[frame0];
			const Keyframe& k1 = boneAnim.Keyframes[frame1];

			//interpolate keyframes
			Keyframe interpolated = AnimationMath::InterpolateKeyframes(k0, k1, interpolation);

			//build Transform MAtrix form interpolated values

			DirectX::XMVECTOR translation = DirectX::XMLoadFloat3(&interpolated.Translation);
			DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&interpolated.Rotation);
			DirectX::XMVECTOR scale = DirectX::XMLoadFloat3(&interpolated.Scale);

			// Build transformation matrix in correct order
			DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScalingFromVector(scale);
			DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(rotation);
			DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslationFromVector(translation);

			// Combine: Scale -> Rotate -> Translate
			DirectX::XMMATRIX transform = DirectX::XMMatrixMultiply(scaleMatrix, rotationMatrix);
			transform = DirectX::XMMatrixMultiply(transform, translationMatrix);

			return transform;
		}

		//Calulate world_space transform by accumalating parent transforms
		void CalculateWorldTransforms(
			const Skeleton& skeleton,
			const std::vector<DirectX::XMMATRIX>& localTransforms,
			std::vector<DirectX::XMMATRIX>& outWorldTransforms)
		{
			for (size_t i = 0; i < skeleton.GetBoneCount(); ++i)
			{
				const Bone& bone = skeleton.GetBone(i);

				if (bone.ParentIndex < 0)
				{
					// Root bone
					outWorldTransforms[i] = localTransforms[i];
				}
				else
				{
					// Child bone: multiply by parent's world transform
					outWorldTransforms[i] = DirectX::XMMatrixMultiply(
						localTransforms[i],
						outWorldTransforms[bone.ParentIndex]);
				}
			}
		}
	};
}
