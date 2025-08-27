#pragma once
#include "camera/Camera.h"
#include <algorithm>
#include "CameraBehavior.h"
namespace DXEngine
{
	class FrameTime;
	class FreeLookBehavior: public CameraBehavior
	{
	public:
		FreeLookBehavior(float sensitivity = 0.002f, float pitchLimit = 1.5f);
		~FreeLookBehavior() = default;

		virtual void Update(Camera& camera, FrameTime deltatime) override;
		
		void HandleMouseInput(int MouseX, int MouseY);


		void SetSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }
		void SetPitchLimit(float limit) { m_PitchLimit = limit; }

		float GetCurrentPitch() const { return m_CurrentPitch; }
		float GetCurrentYaw() const { return m_CurrentYaw; }



	private:
		float m_MouseSensitivity;
		float m_PitchLimit;

		float m_CurrentPitch;
		float m_CurrentYaw;

		float m_LastMouseX;
		float m_LastMouseY;
		bool m_FirstMouse;
	};
}
