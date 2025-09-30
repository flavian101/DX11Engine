#pragma once
#include "CameraBehavior.h"

namespace DXEngine
{
	class FrameTime;
	struct CameraContribution;

	class MovementBehavior : public CameraBehavior
	{
	public:
		MovementBehavior(float moveSpeed = 20.0f);
		~MovementBehavior() = default;

		virtual CameraContribution GetCameraContribution(Camera& camera, FrameTime deltatime) override;

		void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }
		float GetMoveSpeed() { return m_MoveSpeed; }

	private:
		float m_MoveSpeed;
	};

}