#pragma once

namespace DXEngine
{
	class FrameTime;
	class Camera;
	class CameraBehavior
	{
	public:
		CameraBehavior(float priority)
			:m_Priority(priority)
		{}
		virtual ~CameraBehavior() = default;

		virtual void Update(Camera& camera, FrameTime deltaTime) = 0;

		float GetPriority()const { return m_Priority; }
		void SetPriority(bool active) { m_IsActive = active; }

		bool IsActive()const { return m_IsActive; }
		void SetActive(bool active) { m_IsActive; }

	protected:
		float m_Priority;
		bool m_IsActive;
	};
}
