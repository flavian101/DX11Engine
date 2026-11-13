#pragma once
#include <FrameTime.h>
#include <Event/Event.h>
#include <Event/ApplicationEvent.h>
#include <Event/MouseEvent.h>
#include "FreeLookBehavior.h"
#include "MovementBehavior.h"

namespace DXEngine
{
	class Camera;

	class CameraController
	{
	public:
		CameraController();
		~CameraController();

		//const DirectX::XMVECTOR& GetPositionVector() const noexcept;
		const std::shared_ptr<Camera>& GetCamera() const;

		void Move();

		void Update(float dt);
		void OnEvent(Event& event);

		// Keyboard movement and mouse look. Off = the camera stays where it is and ignores the mouse (an editor turns it on
		// only while the user flies the camera, so typing in a text field cannot move it).
		void SetInputEnabled(bool enabled);
		bool IsInputEnabled() const { return m_InputEnabled; }

		// By default the aspect ratio follows the WINDOW. An editor draws the scene into a panel of another shape and sets
		// the aspect itself (Camera::SetAspectRatio), so it switches this off.
		void SetFollowWindowAspect(bool follow) { m_FollowWindowAspect = follow; }

		float moveLeftRight = 0.0f;
		float moveBackForward = 0.0f;

	

	private:
		void OnResize(float width, float Height);
		bool OnWindowResizeEvent(WindowResizeEvent& event);
		bool OnMouseMoveEvent(MouseMovedEvent& event);
	private:
		std::shared_ptr<Camera> m_Camera;
		std::shared_ptr<FreeLookBehavior> m_FreeLookBehavior;
		std::shared_ptr<MovementBehavior> m_MovementBehavior;
		bool m_InputEnabled = true;
		bool m_FollowWindowAspect = true;


	
	};
}
