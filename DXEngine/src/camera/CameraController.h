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

		//const DirectX::XMVECTOR& GetPos() const noexcept;
		const std::shared_ptr<Camera>& GetCamera() const;

		void Move();

		void Update(FrameTime dt);
		void OnEvent(Event& event);

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


	
	};
}
