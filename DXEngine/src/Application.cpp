#include "dxpch.h"
#include "Application.h"
#include "Event/Event.h"
#include "Core/Input.h"

namespace DXEngine {


#define BIND_EVENT_FN(fn) std::bind(&Application::fn, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		// These could be arguments from main(), like window flags or config
		HINSTANCE hInstance = GetModuleHandle(nullptr);
		int nCmdShow = SW_SHOW;

		s_Instance = this;
		m_Window = std::make_unique<Window>(hInstance, nCmdShow, L"DXEngine", L"DirectX", 1270, 720);
		m_Window->SetEventCallback([this](Event& e) { OnEvent(e); });

		Renderer::Init(m_Window->GetHwnd(), m_Window->GetWidth(), m_Window->GetHeight());
		
		m_UILayer = new UILayer();
		PushOverlay(m_UILayer);

		// Pushed LAST so it is the first to see events (events travel the stack from the top).
		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);

	}

	Application::~Application()
	{
		// The UI backend holds device objects: it goes before the renderer does.
		m_ImGuiLayer->OnDetach();
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
		// ONE Mark() per frame; layers get plain seconds. (They used to receive the FrameTime object and every
		// implicit conversion to float called Mark() again, so the value depended on how often it was read.)
		const float deltaSeconds = m_Deltatime.Mark();

		Renderer::SetTime(m_Deltatime.Duration());
		if (!m_Minimized)
		{
			for (Layer* layer : m_LayerStack)
			{
				layer->OnUpdate(deltaSeconds);
			}
		}
		if (m_Minimized)
			return;

		// The UI is built after the scene was drawn (layers draw it in OnUpdate) and goes on top of it.
		m_ImGuiLayer->Begin();
		for (Layer* layer : m_LayerStack)
		{
			layer->OnUIRender();
		}
		m_ImGuiLayer->End();

		// Everything of this frame is drawn: show it. (Renderer::EndScene used to present, which left no way to draw
		// on top of the finished scene; the UI layer needs exactly that.)
		Renderer::Present();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		// Route each type of event to the correct handler:
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResize));
		dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(OnMouseButtonPressedEvent));
		dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(OnMouseButtonReleasedEvent));

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(e);
			if (e.Handled) // if the event is handled, stop processing further
				break;
		}
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}

		m_Minimized = false;

		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
		return false;
	}

	bool Application::OnKeyPressed(KeyPressedEvent& e)
	{
		switch (e.GetKeyCode())
		{
		case VK_ESCAPE:
			if (m_ImGuiLayer->WantsTextInput())    // Escape belongs to the text field being edited
				return false;
			m_Window->QuitWindow();
			return true;
		}
		return false;
	}

	bool Application::OnMouseButtonPressedEvent(MouseButtonPressedEvent& event)
	{
		switch (event.GetMouseButton())
		{
		case VK_MBUTTON:
			if (m_ImGuiLayer->WantsMouse())        // the click is on a panel, not on the scene
				return false;
			m_Window->CaptureCursor();
			m_ImGuiLayer->SetMouseEnabled(false);  // the camera owns the mouse now: the pointer is re-centred every move, panels must not react to it
			return true;
		}
		return false;
	}
	bool Application::OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& event)
	{
		switch (event.GetMouseButton())
		{
		case VK_MBUTTON:
			m_Window->ReleaseCursor();
			m_ImGuiLayer->SetMouseEnabled(true);
			return true;
		}
		return false;
	}
}
