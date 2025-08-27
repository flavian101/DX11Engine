#include "dxpch.h"
#include "FreeLookBehavior.h"
#include "FrameTime.h"

namespace DXEngine
{
	FreeLookBehavior::FreeLookBehavior(float sensitivity, float pitchLimit)
		: CameraBehavior(1.0f)  // High priority for player input
		, m_MouseSensitivity(sensitivity)
		, m_PitchLimit(pitchLimit)
		, m_CurrentPitch(0.0f)
		, m_CurrentYaw(0.0f)
		, m_LastMouseX(0)
		, m_LastMouseY(0)
		, m_FirstMouse(true)
	{
	}
	void FreeLookBehavior::Update(Camera& camera, FrameTime deltatime)
	{
		if (!IsActive())
			return;
		DirectX::XMFLOAT3 currentRotation = camera.GetRotation();

		//TO-DO add blending for complex behaviors
		currentRotation.x = m_CurrentPitch;
		currentRotation.y = m_CurrentYaw;

		camera.SetRotation(currentRotation);
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
		float pitchChange = deltaY * m_MouseSensitivity;

		//update our rotation values
		m_CurrentYaw += yawChange;
		m_CurrentPitch += pitchChange;

		//clamp pith to prevent over-rotation(looking too far up/down)
		m_CurrentPitch = std::max(-m_PitchLimit, std::min(m_PitchLimit, m_CurrentPitch));


		m_LastMouseX = mouseX;
		m_LastMouseY = mouseY;
	}
}