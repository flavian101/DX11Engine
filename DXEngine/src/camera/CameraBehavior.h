#pragma once

namespace DXEngine
{
	class Camera;
	struct CameraContribution;

	class CameraBehavior
	{
	public:
		CameraBehavior(float priority)
			:m_Priority(priority)
		{}
		virtual ~CameraBehavior() = default;

		virtual CameraContribution GetCameraContribution(Camera& camera, float deltaTime) = 0;

		float GetPriority()const { return m_Priority; }
		void SetPriority(float priority) { m_Priority = priority; }

		bool IsActive()const { return m_IsActive; }
		void SetActive(bool active) { m_IsActive = active; }

	protected:
		float m_Priority;
		bool m_IsActive = true;      // was uninitialised: Camera::Update reads it, so a behaviour was active only by luck
	};
}
