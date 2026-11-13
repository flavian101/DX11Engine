#pragma once
#include <FrameTime.h>
#include <functional>
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

		// Keyboard movement and mouse look. Off = the camera stays where it is and ignores the mouse.
		void SetInputEnabled(bool enabled) { m_InputEnabled = enabled; }

		// A question asked LIVE (every frame and for every mouse event) on top of SetInputEnabled: the camera listens only
		// when both say yes. An editor passes "the camera is being flown", so typing in a text field cannot move it and the
		// first mouse move after the button went down is not lost to a flag that updates a frame later.
		using InputGate = std::function<bool()>;
		void SetInputGate(InputGate gate) { m_InputGate = std::move(gate); }
		bool IsInputEnabled() const { return m_InputEnabled && (!m_InputGate || m_InputGate()); }

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
		InputGate m_InputGate;
		bool m_FollowWindowAspect = true;


	
	};
}
