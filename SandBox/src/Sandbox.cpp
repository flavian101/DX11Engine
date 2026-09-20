#include "Sandbox.h"


Sandbox::Sandbox()
	:Layer("sanbox")
{}

Sandbox::~Sandbox()
{
}

void Sandbox::OnAttach()
{
	DXEngine::Renderer::InitLightManager();

	m_CameraController = std::make_shared<DXEngine::CameraController>();
	m_CameraController->GetCamera()->SetPosition({ 0.0f, 20.0f, 50.0f });

	m_Ground = std::make_shared<DXEngine::Ground>();
	m_Ball = std::make_shared<DXEngine::Ball>();
	m_Ball->SetTranslation({ 0.0f,20.0f, 0.0f });
	m_Sky = std::make_shared<DXEngine::SkySphere>();
	m_Light = std::make_shared<DXEngine::LightSphere>();
	m_Board = std::make_shared<DXEngine::Board>();
	m_BallPhysics = std::make_shared<DXEngine::BallPhysics>(DirectX::XMFLOAT3(0.0f, 20.0f, 0.0f),5.0f, 1.0f);
	m_BoardController = std::make_shared<DXEngine::BoardController>();
	InitializePicking();
}

void Sandbox::OnDetach()
{
}

void Sandbox::OnUpdate(float deltaTime)
{
	DXEngine::Renderer::SetClearColor(0.1f, 0.1f, 0.16f);

	m_CameraController->Update(deltaTime);
	DXEngine::Renderer::BeginScene(m_CameraController->GetCamera());
	DetectInput(deltaTime);

	if (m_Sky)
	{
		// Position sky sphere at camera position
		DirectX::XMFLOAT3 camPos = {
			DirectX::XMVectorGetX(m_CameraController->GetCamera()->GetPos()),
			DirectX::XMVectorGetY(m_CameraController->GetCamera()->GetPos()),
			DirectX::XMVectorGetZ(m_CameraController->GetCamera()->GetPos())
		};
		m_Sky->SetTranslation(camPos);
		m_Sky->SetScale({ 50.0f, 50.0f, 50.0f });
		DXEngine::Renderer::Submit(m_Sky);
	
	}

	// Ground
	if (m_Ground)
	{
		m_Ground->SetScale({ 500.0f, 1.0f, 500.0f });
		m_Ground->SetTranslation({ 0.0f,-1.0f, 0.0f });
		//DXEngine::Renderer::Submit(m_Ground);
	}

	DirectX::XMFLOAT3 orientation = m_BoardController->Update(deltaTime);

	m_Ball->SetScale({ 5.0f, 5.0f, 5.0f });
	DirectX::XMFLOAT3 ballPosition = {};
	if (updatePhysics) {
		ballPosition = m_BallPhysics->Update(deltaTime, m_Board->GetModelMatrix());
		m_Ball->SetTranslation(ballPosition);
	}


	m_Board->SetScale({ 20.0f, 1.0f, 20.0f });
	m_Board->SetRotationDegrees(orientation.x, 0.0f, orientation.z);

	DirectX::XMMATRIX boardWorld = m_Board->GetModelMatrix();
	DirectX::XMVECTOR det;
	DirectX::XMMATRIX boardWorldInv = DirectX::XMMatrixInverse(&det, boardWorld);

	DirectX::XMVECTOR ballWorldPos = DirectX::XMLoadFloat3(&ballPosition);

	DirectX::XMVECTOR ballLocalPos = DirectX::XMVector3TransformCoord(ballWorldPos, boardWorldInv);


	DXEngine::Renderer::Submit(m_Board);
	DXEngine::Renderer::Submit(m_Ball);

	

	if (m_Light)
	{
		m_Light->SetTranslation({ 10.0f, 15.0f, 0.0f });
		m_Light->SetScale({ 2.0f, 2.0f, 2.0f });
		DXEngine::Renderer::Submit(m_Light);
	}

	//auto button = std::make_shared<DXEngine::UIButton>("Test Button", DXEngine::UIRect::UIRect(100, 100, 200, 50));
	//button->SetNormalColor(DXEngine::UIColor::UIColor(0.3f, 0.3f, 0.8f, 0.5f));
	//DXEngine::Renderer::SubmitUI(button);
	
	DXEngine::Renderer::EndScene();

}

void Sandbox::OnUIRender()
{

	
}

void Sandbox::OnEvent(DXEngine::Event& event)
{
	m_CameraController->OnEvent(event);
	DXEngine::EventDispatcher dispatcher(event);
	dispatcher.Dispatch<DXEngine::MouseButtonPressedEvent>(DX_BIND_EVENT_FN(Sandbox::OnMouseButtonPressed));
}

bool Sandbox::OnMouseButtonPressed(DXEngine::MouseButtonPressedEvent& e)
{

	if (e.GetMouseButton() == VK_LBUTTON)
	{
		m_LastMouseX = DXEngine::Input::GetMouseX();
		m_LastMouseY = DXEngine::Input::GetMouseY();

		// Handle picking on left mouse button press
		HandlePicking(m_LastMouseX, m_LastMouseY);
	}

	return false;
}
void Sandbox::DetectInput(double time)
{
	static bool wireframeToggled = false;
	if (DXEngine::Input::IsKeyPressed('T'))
	{
		if (!wireframeToggled)
		{
			DXEngine::Renderer::EnableWireframe(!m_WireframeMode);
			m_WireframeMode = !m_WireframeMode;
			wireframeToggled = true;
		}
	}
	else
	{
		wireframeToggled = false;
	}

	// Toggle debug info
	static bool debugToggled = false;
	if (DXEngine::Input::IsKeyPressed('I'))
	{
		if (!debugToggled)
		{
			DXEngine::Renderer::EnableDebugInfo(!mDX_DEBUGMode);
			mDX_DEBUGMode = !mDX_DEBUGMode;
			debugToggled = true;
		}
	}
	else
	{
		debugToggled = false;
	}

	if (DXEngine::Input::IsKeyPressed('U'))
	{
		updatePhysics = true;
	}

	return;
}
void Sandbox::InitializePicking()
{
	m_PickingManager = std::make_unique<DXEngine::PickingManager>();

	// Register pickable objects - cast to InterfacePickable interface
	//if (m_Moon)
	//	m_PickingManager->RegisterPickable(m_Moon);

	if (m_Ground)
		m_PickingManager->RegisterPickable(m_Ground);

	if (m_Light)
		m_PickingManager->RegisterPickable(m_Light);

	// Don't register sky sphere as it should not be pickable
	if (m_Sky)
		m_Sky->SetPickable(false);
}

void Sandbox::HandlePicking(float mouseX, float mouseY)
{
	if (!m_PickingManager || !m_CameraController->GetCamera()) 
		return;

	// Get window dimensions
	//RECT clientRect;
	////GetClientRect(window.GetHwnd(), &clientRect); TO-DO
	//int screenWidth = clientRect.right - clientRect.left;
	//int screenHeight = clientRect.bottom - clientRect.top;
	

	int screenWidth = 1270;
		int screenHeight = 720;
	// Perform picking
	DXEngine::HitInfo hit = m_PickingManager->Pick(mouseX, mouseY, screenWidth, screenHeight, *m_CameraController->GetCamera());

	if (hit.Hit)  // Note: lowercase 'hit'
	{
		// Object was picked
		auto pickedObject = m_PickingManager->GetPickedObject();
		if (pickedObject)
		{
			// Since all your objects inherit from Model, cast to Model first
			// Then check the actual type using the raw pointer approach
			DXEngine::Model* modelPtr = static_cast<DXEngine::Model*>(hit.ObjectPtr);
			if (modelPtr)
			{
				// Check what type of object was picked
				if (dynamic_cast<DXEngine::Ball*>(modelPtr))
				{
					std::cout << "Ball was picked" << std::endl;
				}
				else if (dynamic_cast<DXEngine::Ground*>(modelPtr))
				{
					std::cout << "triangle was picked" << std::endl;
				}
				else if (dynamic_cast<DXEngine::LightSphere*>(modelPtr))
				{
					std::cout << "light sphere was picked" << std::endl;
				}
			}
		}
	}
}