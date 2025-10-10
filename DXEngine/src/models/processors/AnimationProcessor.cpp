#include "dxpch.h"
#include "AnimationProcessor.h"
#include "Animation/AnimationClip.h"
#include <set>


namespace DXEngine
{
	std::vector<std::shared_ptr<AnimationClip>> AnimationProcessor::ProcessAnimations(const aiScene* scene, std::shared_ptr<Skeleton> skeleton)
	{
		std::vector<std::shared_ptr<AnimationClip>> animations;

		if (!scene || !scene->HasAnimations() || !skeleton)
		{
			OutputDebugStringA("AnimationProcessor: No animations or skeleton\n");
			return animations;
		}
		animations.reserve(scene->mNumAnimations);

		for (unsigned int i = 0; i < scene->mNumAnimations; i++)
		{
			const aiAnimation* aiAnim = scene->mAnimations[i];
			auto clip = ProcessAnimation(aiAnim, skeleton);
			if (clip)
			{
				animations.push_back(clip);
				m_AnimationsProcessed++;

				OutputDebugStringA(("AnimationProcessor: Processed '" + clip->GetName() +
					"' - Duration: " + std::to_string(clip->GetDuration()) +
					"s, Channels: " + std::to_string(aiAnim->mNumChannels) + "\n").c_str());

			}
		}

		return animations;
	}
	std::shared_ptr<AnimationClip> AnimationProcessor::ProcessAnimation(const aiAnimation* aiAnim, std::shared_ptr<Skeleton> skeleton)
	{
		if (!aiAnim || !skeleton)
		{
			OutputDebugStringA("AnimationProcessor: Invalid animation or skeleton\n");
			return nullptr;
		}

		//Get animation name
		std::string animName = aiAnim->mName.C_Str();
		if (animName.empty())
		{
			animName = "Animation_" + std::to_string(m_AnimationsProcessed);
		}

		// Calculate duration and ticks per second
		float duration = static_cast<float>(aiAnim->mDuration);
		float ticksPerSecond = static_cast<float>(aiAnim->mTicksPerSecond);

		// Default to 30 fps if not specified
		if (ticksPerSecond <= 0.0f) {
			ticksPerSecond = 30.0f;
			OutputDebugStringA(("AnimationProcessor: No ticks per second, defaulting to 25fps\n"));
		}
		auto clip = std::make_shared<AnimationClip>(animName, duration);
		clip->SetTicksPerSecond(ticksPerSecond);

		// Track statistics
		int processedChannels = 0;
		int skippedChannels = 0;

		// ========================================================================
		// Process each animation channel (bone animation track)
		// ========================================================================
		for (unsigned int i = 0; i < aiAnim->mNumChannels; i++) {
			const aiNodeAnim* channel = aiAnim->mChannels[i];
			std::string boneName = channel->mNodeName.C_Str();

			// Check if this bone exists in our skeleton
			int boneIndex = skeleton->GetBoneIndex(boneName);
			if (boneIndex < 0) {
				skippedChannels++;
#ifdef DX_DEBUG
				OutputDebugStringA(("  Skipping channel for non-skeleton node: " +
					boneName + "\n").c_str());
#endif
				continue;
			}

			// Build keyframes for this bone
			std::vector<Keyframe> keyframes = ProcessChannel(channel);

			// Add bone animation to the clip
			if (!keyframes.empty()) {
				clip->AddBoneAnimation(boneName, keyframes);
				processedChannels++;
			}
		}

		// Validate the clip
		if (processedChannels == 0) {
			OutputDebugStringA(("AnimationProcessor: No valid channels in animation '" +
				animName + "'\n").c_str());
			return nullptr;
		}

		OutputDebugStringA(("  Processed " + std::to_string(processedChannels) +
			" channels, skipped " + std::to_string(skippedChannels) + "\n").c_str());

		return clip;
	}
	std::vector<Keyframe> AnimationProcessor::ProcessChannel(const aiNodeAnim* channel)
	{
		std::vector<Keyframe> keyframes;

		if (!channel)
			return keyframes;

		// ========================================================================
		// Collect all unique timestamps
		// ========================================================================
		std::set<float> allTimes;

		for (unsigned int p = 0; p < channel->mNumPositionKeys; p++)
			allTimes.insert(static_cast<float>(channel->mPositionKeys[p].mTime));

		for (unsigned int r = 0; r < channel->mNumRotationKeys; r++)
			allTimes.insert(static_cast<float>(channel->mRotationKeys[r].mTime));

		for (unsigned int s = 0; s < channel->mNumScalingKeys; s++)
			allTimes.insert(static_cast<float>(channel->mScalingKeys[s].mTime));

		// ========================================================================
		// Create keyframes for each unique time
		// ========================================================================
		keyframes.reserve(allTimes.size());

		for (float time : allTimes) {
			Keyframe keyframe;
			keyframe.TimeStamp = time;

			// Find or interpolate position
			keyframe.Translation = FindOrInterpolatePosition(channel, time);

			// Find or interpolate rotation
			keyframe.Rotation = FindOrInterpolateRotation(channel, time);

			// Find or interpolate scale
			keyframe.Scale = FindOrInterpolateScale(channel, time);

			keyframes.push_back(keyframe);
		}

		return keyframes;
	}
	DirectX::XMFLOAT3 AnimationProcessor::FindOrInterpolatePosition(
		const aiNodeAnim* channel,
		float time)
	{
		// If only one key, return it
		if (channel->mNumPositionKeys == 1)
			return ConvertVector3(channel->mPositionKeys[0].mValue);

		// Find the two keys to interpolate between
		for (unsigned int i = 0; i < channel->mNumPositionKeys - 1; i++) {
			float t0 = static_cast<float>(channel->mPositionKeys[i].mTime);
			float t1 = static_cast<float>(channel->mPositionKeys[i + 1].mTime);

			if (time >= t0 && time < t1) {
				// Linear interpolation
				float factor = (time - t0) / (t1 - t0);
				const aiVector3D& v0 = channel->mPositionKeys[i].mValue;
				const aiVector3D& v1 = channel->mPositionKeys[i + 1].mValue;

				aiVector3D result = v0 + (v1 - v0) * factor;
				return ConvertVector3(result);
			}
		}

		// Return last key if time is beyond animation
		return ConvertVector3(channel->mPositionKeys[channel->mNumPositionKeys - 1].mValue);
	}

	DirectX::XMFLOAT4 AnimationProcessor::FindOrInterpolateRotation(
		const aiNodeAnim* channel,
		float time)
	{
		// If only one key, return it
		if (channel->mNumRotationKeys == 1)
			return ConvertQuaternion(channel->mRotationKeys[0].mValue);

		// Find the two keys to interpolate between
		for (unsigned int i = 0; i < channel->mNumRotationKeys - 1; i++) {
			float t0 = static_cast<float>(channel->mRotationKeys[i].mTime);
			float t1 = static_cast<float>(channel->mRotationKeys[i + 1].mTime);

			if (time >= t0 && time < t1) {
				// Spherical linear interpolation (SLERP)
				float factor = (time - t0) / (t1 - t0);
				aiQuaternion q0 = channel->mRotationKeys[i].mValue;
				aiQuaternion q1 = channel->mRotationKeys[i + 1].mValue;

				aiQuaternion result;
				aiQuaternion::Interpolate(result, q0, q1, factor);
				result.Normalize();

				return ConvertQuaternion(result);
			}
		}

		// Return last key if time is beyond animation
		return ConvertQuaternion(channel->mRotationKeys[channel->mNumRotationKeys - 1].mValue);
	}

	DirectX::XMFLOAT3 AnimationProcessor::FindOrInterpolateScale(
		const aiNodeAnim* channel,
		float time)
	{
		// If only one key, return it
		if (channel->mNumScalingKeys == 1)
			return ConvertVector3(channel->mScalingKeys[0].mValue);

		// Find the two keys to interpolate between
		for (unsigned int i = 0; i < channel->mNumScalingKeys - 1; i++) {
			float t0 = static_cast<float>(channel->mScalingKeys[i].mTime);
			float t1 = static_cast<float>(channel->mScalingKeys[i + 1].mTime);

			if (time >= t0 && time < t1) {
				// Linear interpolation
				float factor = (time - t0) / (t1 - t0);
				const aiVector3D& v0 = channel->mScalingKeys[i].mValue;
				const aiVector3D& v1 = channel->mScalingKeys[i + 1].mValue;

				aiVector3D result = v0 + (v1 - v0) * factor;
				return ConvertVector3(result);
			}
		}

		// Return last key if time is beyond animation
		return ConvertVector3(channel->mScalingKeys[channel->mNumScalingKeys - 1].mValue);
	}

	// ============================================================================
	// CONVERSION UTILITIES
	// ============================================================================

	DirectX::XMFLOAT3 AnimationProcessor::ConvertVector3(const aiVector3D& vector)
	{
		return DirectX::XMFLOAT3(vector.x, vector.y, vector.z);
	}

	DirectX::XMFLOAT4 AnimationProcessor::ConvertQuaternion(const aiQuaternion& quat)
	{
		return DirectX::XMFLOAT4(quat.x, quat.y, quat.z, quat.w);
	}

}