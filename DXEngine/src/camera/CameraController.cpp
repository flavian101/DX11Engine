#include "dxpch.h"
#include "DXEngine.h"
#include "camera/Camera.h"
#include "CameraController.h"
#include "renderer/RendererCommand.h"
#include <Core/Input.h>

namespace DXEngine
{
	CameraController::CameraController()
		:
		m_Camera(std::make_shared<DXEngine::Camera>())
	{
		m_Camera->SetProjectionParams(45.0f, (float)16.0f / (float)9.0f, 0.5f, 1000.0f);
		m_FreeLookBehavior = std::make_shared<FreeLookBehavior>(0.002f, 1.5f);
		m_Camera->AddBehaviour(m_FreeLookBehavior);
	}

	CameraController::~CameraController()
	{
	}

	//const DirectX::XMVECTOR& CameraController::GetPos() const noexcept
	//{
	//	return 
	//}

	const std::shared_ptr<Camera>& CameraController::GetCamera() const
	{
		return m_Camera;
	}

	void CameraController::Move()
	{
	}

	void CameraController::Update(FrameTime dt)
	{
		m_Camera->Update(dt);

		float speed = dt * 60000.5f;

		if (Input::IsKeyPressed('W'))
		{
			moveBackForward += speed;
		}
		if (Input::IsKeyPressed('S'))
		{
			moveBackForward -= speed;
		}
		if (Input::IsKeyPressed('A'))
		{
			moveLeftRight -= speed;
		}
		if (Input::IsKeyPressed('D'))
		{
			moveLeftRight += speed;
		}


	}

	void CameraController::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<WindowResizeEvent>(DX_BIND_EVENT_FN(CameraController::OnWindowResizeEvent));
		dispatcher.Dispatch<MouseMovedEvent>(DX_BIND_EVENT_FN(CameraController::OnMouseMoveEvent));
	}

	void CameraController::OnResize(float width, float height)
	{
		float aspectRatio = (float)width / (float)height;
		//m_Camera->SetAspectRatio(aspectRatio);
	}

	bool CameraController::OnWindowResizeEvent(WindowResizeEvent& event)
	{
		OnResize((float)event.GetWidth(), (float)event.GetHeight());
		return false;
	}
	bool CameraController::OnMouseMoveEvent(MouseMovedEvent& event)
	{
		m_FreeLookBehavior->HandleMouseInput(event.GetX(), event.GetY());
		std::cout << "mouse moved event (" << event.GetX() << ", " << event.GetY() << ")" << std::endl;
		return false;
	}
}