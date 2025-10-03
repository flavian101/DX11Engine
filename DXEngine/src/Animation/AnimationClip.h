#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <map>

namespace DXEngine
{
	//Represent a single keyframe of a bone
	struct Keyframe
	{
		float TimeStamp;
		DirectX::XMFLOAT3 Translation;
		DirectX::XMFLOAT4 Rotation;
		DirectX::XMFLOAT3 Scale;

		Keyframe()
			:
			TimeStamp(0.0f),
			Translation(0.0f, 0.0f, 0.0f),
			Rotation(0.0f, 0.0f, 0.0f, 1.0f),
			Scale(1.0f, 1.0f, 1.0f)
		{}
	};

	//Animation Track for a single Bone
	struct BoneAnimation
	{
		std::string BoneName;
		std::vector<Keyframe> Keyframes;
		
		// Helper to find keyframes for interpolation
		void GetFrameIndices(float time, size_t& frame0, size_t& frame1, float& interpolation)const
		{
			if (Keyframes.empty())
			{
				frame0 = frame1 = 0;
				interpolation = 0.0f;
				return;
			}

			//find the two keyframes to interpolate between 
			for (size_t i = 0; i < Keyframes.size() - 1; i++)
			{
				if (time >= Keyframes[i].TimeStamp && time < Keyframes[i + 1].TimeStamp)
				{
					frame0 = i;
					frame1 = i + 1;
					float dt = Keyframes[i + 1].TimeStamp - Keyframes[i].TimeStamp;
					interpolation = (time - Keyframes[i].TimeStamp) / dt;
					return;
				}
			}

			//if we're past the last keyframe, clamp to last frame
			frame0 = frame1 = Keyframes.size() - 1;
			interpolation = 0.0f;
		}
	};

	class AnimationClip
	{
	public:
		AnimationClip(const std::string& name, float duration)
			: m_Name(name)
			, m_Duration(duration)
			, m_TicksPerSecond(30.0f)  // Default 30 FPS
		{}

		//Add Animation data for a bone
		void AddBoneAnimation(const std::string& boneName, const std::vector<Keyframe>& keyframes)
		{
			BoneAnimation boneAnim;
			boneAnim.BoneName = boneName;
			boneAnim.Keyframes = keyframes;
			m_BoneAnimations[boneName] = boneAnim;
		}

		//get animation for a specific bone
		const BoneAnimation* GetBoneAnimation(const std::string& boneName) const
		{
			auto it = m_BoneAnimations.find(boneName);
			return (it != m_BoneAnimations.end()) ? &it->second : nullptr;
		}
		// Properties
		const std::string& GetName() const { return m_Name; }
		float GetDuration() const { return m_Duration; }
		float GetTicksPerSecond() const { return m_TicksPerSecond; }
		void SetTicksPerSecond(float tps) { m_TicksPerSecond = tps; }

		size_t GetBoneCount() const { return m_BoneAnimations.size(); }
		const std::map<std::string, BoneAnimation>& GetBoneAnimations() const { return m_BoneAnimations; }

	private:
		std::string m_Name;
		float m_Duration;
		float m_TicksPerSecond;
		std::map<std::string, BoneAnimation> m_BoneAnimations;	
	};

	//bone Hierachy or simple skeleton
	struct Bone
	{
		std::string Name;
		int ParentIndex;   //-1 root bone
		DirectX::XMFLOAT4X4 OffsetMatrix;            // Transform from mesh space to bone space
		DirectX::XMFLOAT4X4 LocalTransform;          // Local transform relative to parent

		Bone()
			:ParentIndex(-1)
		{
			DirectX::XMStoreFloat4x4(&OffsetMatrix, DirectX::XMMatrixIdentity());
			DirectX::XMStoreFloat4x4(&LocalTransform, DirectX::XMMatrixIdentity());
		}
	};


	class Skeleton
	{
	public:
		void AddBone(const Bone& bone)
		{
			m_Bones.push_back(bone);
			m_BoneNameToIndex[bone.Name] = m_Bones.size() - 1;
		}

		size_t GetBoneCount()const { return m_Bones.size(); }
		const Bone& GetBone(size_t index)const { return m_Bones[index]; }
		Bone& GetBone(size_t index) { return m_Bones[index]; }

		int GetBoneIndex(const std::string& boneName) const
		{
			auto it = m_BoneNameToIndex.find(boneName);
			return (it != m_BoneNameToIndex.end()) ? it->second : -1;
		}

		const std::vector<Bone>& GetBones()const { return m_Bones; }
	private:
		std::vector<Bone> m_Bones;
		std::map<std::string, int> m_BoneNameToIndex;
	};

	
}