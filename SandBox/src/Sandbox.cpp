#include "Sandbox.h"


Sandbox::Sandbox()
	:Layer("sanbox"),
	camera(std::make_shared<DXEngine::Camera>(1.0f,  9.0f/ 16.0f, 0.5f, 100.0f))

{}

Sandbox::~Sandbox()
{
}

void Sandbox::OnAttach()
{
	m_ShaderManager = std::make_shared<DXEngine::ShaderManager>();
	m_ShaderManager->Initialize();

	DXEngine::Renderer::InitShaderManager(m_ShaderManager);

	tri = std::make_shared<DXEngine::Ground>();
	ball = std::make_shared<DXEngine::Ball>();
	sky = std::make_shared<DXEngine::SkySphere>();
	m_Light = std::make_shared<DXEngine::LightSphere>();

	InitializePicking();
}

void Sandbox::OnDetach()
{
}

void Sandbox::OnUpdate(DXEngine::FrameTime dt)
{
	DXEngine::Renderer::SetClearColor(0.1f, 0.1f, 0.16f);

	//window.().ClearDepthColor(0.1f, 0.1f, 0.16f);

	DXEngine::Renderer::BeginScene(camera);
	DetectInput(dt);

	if (sky)
	{
		// Position sky sphere at camera position
		DirectX::XMFLOAT3 camPos = {
			DirectX::XMVectorGetX(camera->GetPos()),
			DirectX::XMVectorGetY(camera->GetPos()),
			DirectX::XMVectorGetZ(camera->GetPos())
		};
		sky->SetTranslation(camPos);
		sky->SetScale({ 50.0f, 50.0f, 50.0f });
		DXEngine::Renderer::Submit(sky);

	}

	// Ground
	if (tri)
	{
		tri->SetScale({ 500.0f, 10.0f, 500.0f });
		tri->SetTranslation({ 0.0f,0.0f, 0.0f });
		DXEngine::Renderer::Submit(tri);
	}

	// Moon/Ball
	if (ball)
	{
		//ball->SetScale({ 10.0f, 10.0f, 10.0f });
		ball->SetTranslation({ 0.0f, 5.0f, 0.0f });
		DXEngine::Renderer::Submit(ball);
	}

	// Light sphere
	if (m_Light)
	{
		m_Light->SetTranslation({ 0.0f, 10.0f, 0.0f });
		m_Light->SetScale({ 1.0f, 1.0f, 1.0f });
		m_Light->BindLight();
		DXEngine::Renderer::Submit(m_Light);
	}
	DXEngine::Renderer::EndScene();

}

void Sandbox::OnUIRender()
{

	
}

void Sandbox::OnEvent(DXEngine::Event& event)
{
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
	float speed = time * 300.5f;

	if (DXEngine::Input::IsKeyPressed('W'))
	{
		camera->moveBackForward += speed;
	}
	if (DXEngine::Input::IsKeyPressed('S'))
	{
		camera->moveBackForward -= speed;
	}
	if (DXEngine::Input::IsKeyPressed('A'))
	{
		camera->moveLeftRight -= speed;
	}
	if (DXEngine::Input::IsKeyPressed('D'))
	{
		camera->moveLeftRight += speed;
	}

	// Toggle wireframe mode
	if (DXEngine::Input::IsKeyPressed('T'))
	{
		static bool wireframeToggled = false;
		if (!wireframeToggled)
		{
			DXEngine::Renderer::EnableWireframe(!m_WireframeMode);
			m_WireframeMode = !m_WireframeMode;
			wireframeToggled = true;
		}
	}
	else
	{
		static bool wireframeToggled = false;
		wireframeToggled = false;
	}

	// Toggle debug info
	if (DXEngine::Input::IsKeyPressed('I'))
	{
		static bool debugToggled = false;
		if (!debugToggled)
		{
			DXEngine::Renderer::EnableDebugInfo(!m_DebugMode);
			m_DebugMode = !m_DebugMode;
			debugToggled = true;
		}
	}
	else
	{
		static bool debugToggled = false;
		debugToggled = false;
	}

	camera->UpdateCamera();
	//call update

	return;
}
void Sandbox::InitializePicking()
{
	m_PickingManager = std::make_unique<DXEngine::PickingManager>();

	// Register pickable objects - cast to InterfacePickable interface
	if (ball)
		m_PickingManager->RegisterPickable(ball);

	if (tri)
		m_PickingManager->RegisterPickable(tri);

	if (m_Light)
		m_PickingManager->RegisterPickable(m_Light);

	// Don't register sky sphere as it should not be pickable
	if (sky)
		sky->SetPickable(false);
}

void Sandbox::HandlePicking(float mouseX, float mouseY)
{
	if (!m_PickingManager || !camera)
		return;

	// Get window dimensions
	//RECT clientRect;
	////GetClientRect(window.GetHwnd(), &clientRect); TO-DO
	//int screenWidth = clientRect.right - clientRect.left;
	//int screenHeight = clientRect.bottom - clientRect.top;
	

	int screenWidth = 1270;
		int screenHeight = 720;
	// Perform picking
	DXEngine::HitInfo hit = m_PickingManager->Pick(mouseX, mouseY, screenWidth, screenHeight, *camera);

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