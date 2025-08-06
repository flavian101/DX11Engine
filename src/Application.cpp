#include "Application.h"
#include <optional>
#include "Event/Event.h"
#include <functional>
#include "Core/Input.h"
#include <iostream>

namespace DXEngine {


#define BIND_EVENT_FN(fn) std::bind(&Application::fn, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;

	Application::Application(HINSTANCE hInstance, int showWnd)
	{
		s_Instance = this;
		m_Window = std::make_unique<Window>(hInstance, showWnd, L"DXEngine", L"DirectX", 1270, 720);
		m_Window->SetEventCallback([this](Event& e) { OnEvent(e); });

		Renderer::Init(m_Window->GetHwnd(), m_Window->GetWidth(), m_Window->GetHeight());

		// Set clear color
		Renderer::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);


   // m(window.());

}

Application::~Application()
{
	Renderer::Shutdown();

}

int Application::createLoop()
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

void Application::PushLayer(Layer* layer)
{
	m_LayerStack.PushLayer(layer);
	layer->OnAttach(); // Call OnAttach for the layer when it is pushed
}
void Application::PushOverlay(Layer* overlay)
{
	m_LayerStack.PushOverlay(overlay);
	overlay->OnAttach(); // Call OnAttach for the overlay when it is pushed
}


void Application::Render()
{
	FrameTime deltatime;

	if (!m_Minimized)
	{
		for (Layer* layer : m_LayerStack)
		{
			layer->OnUpdate(deltatime);
		}
	}
	//m_ImGuiLayer->Begin();
	//for (Layer* layer : m_LayerStack)
	//{
	//	layer->OnImGuiRender();
	//}
	//m_ImGuiLayer->End();
	//m_Window->OnUpdate();

	RenderCommand::Clear();
	Renderer::EndScene();
	
}

void Application::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	// Route each type of event to the correct handler:
	dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResize));
	dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(OnKeyPressed));
	dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(OnMouseMoved));
	dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(OnMouseScrolled));
	dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(OnMouseButtonPressed));
	dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(OnMouseButtonReleased));


	for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
	{
		(*--it)->OnEvent(e);
		if (e.Handled) // if the event is handled, stop processing further
			break;
	}
}

bool Application::OnWindowResize(WindowResizeEvent& e)
{
	Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
	return true;
}

bool Application::OnKeyPressed(KeyPressedEvent& e)
{
	switch (e.GetKeyCode())
	{
	case VK_ESCAPE:
		m_Window->QuitWindow();
		return true;
	//case 'W':
	//	//camera->MoveForward();
	//	return true;
	//	// … handle other keys …
	}
	return true;  
}

bool Application::OnMouseMoved(MouseMovedEvent& e)
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

bool Application::OnMouseScrolled(MouseScrolledEvent& e)
{
	
	return true;
}



bool Application::OnMouseButtonReleased(MouseButtonReleasedEvent& e)
{
	return false;
}

}