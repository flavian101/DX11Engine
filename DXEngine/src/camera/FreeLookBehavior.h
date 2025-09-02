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
		FreeLookBehavior(float sensitivity = 0.002f);
		~FreeLookBehavior() = default;

		virtual CameraContribution GetCameraContribution(Camera& camera, FrameTime deltatime) override;
		
		void HandleMouseInput(int MouseX, int MouseY, bool isCaptured);

		void SetSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }

		float GetFramePitchChange() const { return m_FramePitchChange; }
		float GetFrameYawChange() const { return m_FrameYawChange; }



	private:
		float m_MouseSensitivity;

		float m_LastMouseX;
		float m_LastMouseY;
		bool m_FirstMouse;

		float m_FrameYawChange;
		float m_FramePitchChange;
	};
}
