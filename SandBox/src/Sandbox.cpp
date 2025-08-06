#include "Sandbox.h"

Sandbox::Sandbox()
	:
	camera(std::make_shared<DXEngine::Camera>(1.0f, 9.0f / 16.0f, 0.5f, 100.0f))

{
	//DXEngine::RenderCommand::SetCamera(camera);
	//m_Window.().SetCamera(camera);
	
}

Sandbox::~Sandbox()
{
}

void Sandbox::OnAttach()
{
	m_ShaderManager = std::make_shared<DXEngine::ShaderManager>();

	tri = std::make_shared<DXEngine::Triangle>(m_ShaderManager->GetShaderProgram("DiffuseNormal"));
	ball = std::make_shared<DXEngine::Ball>(m_ShaderManager->GetShaderProgram("DiffuseNormal"));
	sky = std::make_shared<DXEngine::SkySphere>( m_ShaderManager->GetShaderProgram("SkyShader"));
	m_Light = std::make_shared<DXEngine::LightSphere>(m_ShaderManager->GetShaderProgram("Flat"));
}

void Sandbox::OnDetach()
{
}

void Sandbox::OnUpdate(DXEngine::FrameTime dt)
{
	DetectInput(dt);

	//window.().ClearDepthColor(0.1f, 0.1f, 0.16f);


	sky->Render();


	tri->SetScale({ 500.0f, 10.0f, 500.0f });
	tri->SetTranslation({ 0.0f, 10.0f, 0.0f });
	tri->Render();

	m_Light->SetTranslation({ 0.0f, 10.0f, 0.0f });
	m_Light->Render();

	ball->SetScale({ 10.0f, 10.0f, 10.0f });
	ball->SetTranslation({ 30.0f, 30.0f,20.0f });
	ball->Render();

}

void Sandbox::OnUIRender()
{

	
}

void Sandbox::OnEvent(DXEngine::Event& event)
{
}

//bool Sandbox::OnMouseButtonPressed(DXEngine::MouseButtonPressedEvent& e)
//{
//
//	if (e.GetMouseButton() == VK_LBUTTON)
//	{
//		m_LastMouseX = DXEngine::Input::GetMouseX();
//		m_LastMouseY = DXEngine::Input::GetMouseY();
//
//		// Handle picking on left mouse button press
//		HandlePicking(m_LastMouseX, m_LastMouseY);
//	}
//
//	return false;
//}
void Sandbox::DetectInput(double time)
{
	float speed = time * 0.025f;
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

	int screenWidth = 200;
		int screenHeight = 200;
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
					OutputDebugStringA("Ball picked!\n");
				}
				else if (dynamic_cast<DXEngine::Triangle*>(modelPtr))
				{
					OutputDebugStringA("Triangle picked!\n");
				}
				else if (dynamic_cast<DXEngine::LightSphere*>(modelPtr))
				{
					OutputDebugStringA("Light picked!\n");
				}
			}
		}
	}
}