#include "dxpch.h"
#include "FreeLookBehavior.h"
#include "FrameTime.h"

namespace DXEngine
{
	FreeLookBehavior::FreeLookBehavior(float sensitivity)
		: CameraBehavior(1.0f)  // High priority for player input
		, m_MouseSensitivity(sensitivity)
		, m_FramePitchChange(0.0f)
		, m_FrameYawChange(0.0f)
		, m_LastMouseX(0)
		, m_LastMouseY(0)
		, m_FirstMouse(true)
	{
	}
	RotationContribution FreeLookBehavior::GetRotationContribution(Camera& camera, FrameTime deltatime)
	{
		if (!IsActive())
			return RotationContribution({0.0f,0.0f,0.0f}, 0.0f);

		DirectX::XMFLOAT3 rotationChange(m_FramePitchChange, m_FrameYawChange, 0.0f);
		
		float weight = GetPriority();

		m_FrameYawChange = 0.0f;
		m_FramePitchChange = 0.0f;


		return RotationContribution(rotationChange, weight);
	}
	void FreeLookBehavior::HandleMouseInput(int mouseX, int mouseY)
	{
		//skip the first mouse to avoid jumping 
		if (m_FirstMouse)
		{
			m_LastMouseX = mouseX;
			m_LastMouseY = mouseY;
			m_FirstMouse = false;
			return;
		}

		//calculate mose movement
		int deltaX = mouseX - m_LastMouseX;
		int deltaY = mouseY - m_LastMouseY;

		//convert pixel movemnt to rotation changes
		float yawChange = deltaX * m_MouseSensitivity;
		float pitchChange = -deltaY * m_MouseSensitivity;

		//update our rotation values
		m_FrameYawChange = yawChange;
		m_FramePitchChange = pitchChange;

		m_LastMouseX = mouseX;
		m_LastMouseY = mouseY;
	}
}