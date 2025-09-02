#include "dxpch.h"
#include "DXEngine.h"
#include "camera/Camera.h"
#include "CameraController.h"
#include "renderer/RendererCommand.h"

namespace DXEngine
{
	CameraController::CameraController()
		:
		m_Camera(std::make_shared<DXEngine::Camera>())
	{
		m_Camera->SetProjectionParams(45.0f, (float)16.0f / (float)9.0f, 0.5f, 1000.0f);
		m_FreeLookBehavior = std::make_shared<FreeLookBehavior>(0.002f);
		m_MovementBehavior = std::make_shared<MovementBehavior>(5000.0f);

		m_Camera->AddBehaviour(m_FreeLookBehavior);
		m_Camera->AddBehaviour(m_MovementBehavior);
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
		m_Camera->SetAspectRatio(aspectRatio);
	}

	bool CameraController::OnWindowResizeEvent(WindowResizeEvent& event)
	{
		OnResize((float)event.GetWidth(), (float)event.GetHeight());
		return false;
	}
	bool CameraController::OnMouseMoveEvent(MouseMovedEvent& event)
	{
		m_FreeLookBehavior->HandleMouseInput(event.GetX(), event.GetY(),event.GetIsCaptured());
		return false;
	}

}