#include "App.h"
#include <optional>
#include "Event/Event.h"
#include <functional>
#include "Core/Input.h"
#include <iostream>



#define BIND_EVENT_FN(fn) std::bind(&App::fn, this, std::placeholders::_1)

App::App(HINSTANCE hInstance, int showWnd)
    :
    window(hInstance, showWnd, L"engine", L"DirectX", 1270, 720),
	camera(std::make_shared<Camera>(1.0f, 9.0f / 16.0f, 0.5f, 100.0f)) 
{
	window.SetEventCallback([this](Event& e) { OnEvent(e); });

	window.Gfx().SetCamera(camera);

	m_ShaderManager = std::make_shared<ShaderManager>(window.Gfx());

	tri = std::make_shared<Triangle>(window.Gfx(),m_ShaderManager->GetShaderProgram("DiffuseNormal"));
	ball = std::make_shared<Ball>(window.Gfx(),m_ShaderManager->GetShaderProgram("DiffuseNormal"));
	sky = std::make_shared<SkySphere>(window.Gfx(), m_ShaderManager->GetShaderProgram("SkyShader"));
	m_Light = std::make_shared<LightSphere>(window.Gfx(),m_ShaderManager->GetShaderProgram("Flat"));
   // m(window.Gfx());

	InitializePicking();
}

App::~App()
{
}

int App::createLoop()
{
    while (true)
    {
        //process all messages pending 
        if (const auto ecode = Window::ProcessMessages())
        {
            //if return optional has value, means we'are exiting the program by returning the exit code
            return *ecode;
        }
        Render();
	}


	
}

void App::Render()
{
	const float t = timer.Peek();

	window.Gfx().ClearDepthColor(0.1f, 0.1f, 0.16f);

	DetectInput(t);

	sky->Render(window.Gfx());


	tri->SetScale({ 500.0f, 10.0f, 500.0f });
	tri->SetTranslation({ 0.0f, 10.0f, 0.0f });
	tri->Render(window.Gfx());

	m_Light->SetTranslation({ 0.0f, 10.0f, 0.0f });
	m_Light->Render(window.Gfx());

	ball->SetScale({ 10.0f, 10.0f, 10.0f });
	ball->SetTranslation({ 30.0f, 30.0f,20.0f});
	ball->Render(window.Gfx());
	
    window.Gfx().End();
	
}

void App::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	// Route each type of event to the correct handler:
	dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResize));
	dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(OnKeyPressed));
	dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(OnMouseMoved));
	dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(OnMouseScrolled));
	dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(OnMouseButtonPressed));
	dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(OnMouseButtonReleased));
}

bool App::OnWindowResize(WindowResizeEvent& e)
{
	window.Gfx().Resize(e.GetWidth(), e.GetHeight());

	// also update the camera’s aspect ratio
	float width = e.GetWidth();
	float height = e.GetHeight();
	float aspect = height/ width ;
	XMMATRIX camProjection= DirectX::XMMatrixPerspectiveLH(1.0f, aspect, 0.5f, 100.0f);
	camera->SetProjection(camProjection);
	return true;
}

bool App::OnKeyPressed(KeyPressedEvent& e)
{
	switch (e.GetKeyCode())
	{
	case VK_ESCAPE:
		window.QuitWindow();
		return true;
	//case 'W':
	//	//camera->MoveForward();
	//	return true;
	//	// … handle other keys …
	}
	return true;  
}

bool App::OnMouseMoved(MouseMovedEvent& e)
{
	//float mousePosX = Input::GetMouseX();
	//float mousePosY = Input::GetMouseY();
	//
	//camera->camPitch += mousePosX * 0.00001f;
	//camera->camYaw += mousePosY * 0.00001f;
	//
	//camera->UpdateCamera();

	return true;
}

bool App::OnMouseScrolled(MouseScrolledEvent& e)
{
	
	return true;
}

bool App::OnMouseButtonPressed(MouseButtonPressedEvent& e)
{

	if (e.GetMouseButton() == VK_LBUTTON)
	{
		m_LastMouseX = Input::GetMouseX();
		m_LastMouseY = Input::GetMouseY();
		
		// Handle picking on left mouse button press
		HandlePicking(m_LastMouseX, m_LastMouseY);
	}

	return false;
}

bool App::OnMouseButtonReleased(MouseButtonReleasedEvent& e)
{
	return false;
}

void App::DetectInput(double time)
{
	float speed = time * 0.025f;
	if (Input::IsKeyPressed('W'))
	{
		camera->moveBackForward += speed;
	}
	if (Input::IsKeyPressed('S'))
	{
		camera->moveBackForward -= speed;
	}
	if (Input::IsKeyPressed('A'))
	{
		camera->moveLeftRight -= speed;
	}
	if (Input::IsKeyPressed('D'))
	{
		camera->moveLeftRight += speed;
	}

	camera->UpdateCamera();
	//call update

	return;
}
void App::InitializePicking()
{
	m_PickingManager = std::make_unique<PickingManager>();

	// Register pickable objects - cast to IPickable interface
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

void App::HandlePicking(float mouseX, float mouseY)
{
	if (!m_PickingManager || !camera)
		return;

	// Get window dimensions
	RECT clientRect;
	GetClientRect(window.GetHwnd(), &clientRect);
	int screenWidth = clientRect.right - clientRect.left;
	int screenHeight = clientRect.bottom - clientRect.top;

	// Perform picking
	HitInfo hit = m_PickingManager->Pick(mouseX, mouseY, screenWidth, screenHeight, *camera);

	if (hit.Hit)  // Note: lowercase 'hit'
	{
		// Object was picked
		auto pickedObject = m_PickingManager->GetPickedObject();
		if (pickedObject)
		{
			// Since all your objects inherit from Model, cast to Model first
			// Then check the actual type using the raw pointer approach
			Model* modelPtr = static_cast<Model*>(hit.ObjectPtr);
			if (modelPtr)
			{
				// Check what type of object was picked
				if (dynamic_cast<Ball*>(modelPtr))
				{
					OutputDebugStringA("Ball picked!\n");
				}
				else if (dynamic_cast<Triangle*>(modelPtr))
				{
					OutputDebugStringA("Triangle picked!\n");
				}
				else if (dynamic_cast<LightSphere*>(modelPtr))
				{
					OutputDebugStringA("Light picked!\n");
				}
			}
		}
	}
}