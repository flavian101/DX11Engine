#pragma once
#include "AnimationClip.h"
#include "AnimationEvaluator.h"
#include <memory>
#include "FrameTime.h"

namespace DXEngine
{
	enum class PlaybackMode
	{
		Once,		//play once
		Loop,		//loop continuosly
		PingPong,	//play forward, then backward
	};

	class AnimationController
	{
	public:
		AnimationController(std::shared_ptr<Skeleton> skeleton)
			: m_Skeleton(skeleton)
			, m_CurrentClip(nullptr)
			, m_CurrentTime(0.0f)
			, m_PlaybackSpeed(1.0f)
			, m_IsPlaying(false)
			, m_Mode(PlaybackMode::Loop)
			, m_IsReversed(false)
		{}

		//set The current Clip
		void SetClip(std::shared_ptr<AnimationClip> clip)
		{
			m_CurrentClip = clip;
			m_CurrentTime = 0.0f;
			m_IsReversed = false;
		}

		//playback controls
		void Play()
		{
			m_IsPlaying = true;
		}

		void Pause()
		{
			m_IsPlaying = false;
		}

		void Stop()
		{
			m_IsPlaying = false;
			m_CurrentTime = 0.0f;
			m_IsReversed = false;
		}

		void Reset()
		{
			m_CurrentTime = 0.0f;
			m_IsReversed = false;
		}

		//update Animation (calls Every frame)
		void Update(FrameTime deltatime)
		{
			if (!m_IsPlaying || !m_CurrentClip)
				return;

			float direction = m_IsReversed ? -1.0f : 1.0f;
			m_CurrentTime += deltatime * m_PlaybackSpeed * direction;

			float duration = m_CurrentClip->GetDuration();

			//Handle Different playback modes
			switch (m_Mode)
			{
			case PlaybackMode::Once:
				if (m_CurrentTime >= duration)
				{
					m_CurrentTime = duration;
					m_IsPlaying = false;
				}
				else if(m_CurrentTime < 0.0f)
				{
					m_CurrentTime = 0.0f;
					m_IsPlaying = false;
				}
				break;
			case PlaybackMode::Loop:
				if (m_CurrentTime >= duration)
				{
					m_CurrentTime = fmod(m_CurrentTime, duration);
				}
				else if (m_CurrentTime < 0.0f)
				{
					m_CurrentTime = duration + fmod(m_CurrentTime, duration);
				}
				break;
			case PlaybackMode::PingPong:
				if (m_CurrentTime >= duration)
				{
					m_CurrentTime = duration;
					m_IsReversed = true;  // Reverse direction
				}
				else if (m_CurrentTime <= 0.0f)
				{
					m_CurrentTime = 0.0f;
					m_IsReversed = false;  // Forward again
				}
				break;
			}
		}

		//Get Current Bone Matrices for Rendering 
		const std::vector<DirectX::XMFLOAT4X4>& GetBoneMatrices()
		{
			if (m_CurrentClip && m_Skeleton)
			{
				m_Evaluator.Evaluate(*m_CurrentClip, *m_Skeleton, m_CurrentTime, m_BoneMatrices);
			}
			return m_BoneMatrices;
		}

		// Settings
		void SetPlaybackSpeed(float speed) { m_PlaybackSpeed = speed; }
		float GetPlaybackSpeed() const { return m_PlaybackSpeed; }

		void SetPlaybackMode(PlaybackMode mode) { m_Mode = mode; }
		PlaybackMode GetPlaybackMode() const { return m_Mode; }

		// State queries
		bool IsPlaying() const { return m_IsPlaying; }
		float GetCurrentTime() const { return m_CurrentTime; }
		float GetNormalizedTime() const
		{
			return m_CurrentClip ? (m_CurrentTime / m_CurrentClip->GetDuration()) : 0.0f;
		}

		const std::shared_ptr<AnimationClip>& GetCurrentClip() const { return m_CurrentClip; }
		const std::shared_ptr<Skeleton>& GetSkeleton() const { return m_Skeleton; }

	private:
		std::shared_ptr<Skeleton> m_Skeleton;
		std::shared_ptr<AnimationClip> m_CurrentClip;
		AnimationEvaluator m_Evaluator;

		std::vector<DirectX::XMFLOAT4X4> m_BoneMatrices;

		float m_CurrentTime;
		float m_PlaybackSpeed;
		bool m_IsPlaying;
		bool m_IsReversed;
		PlaybackMode m_Mode;
	};
}