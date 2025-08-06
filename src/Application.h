#pragma once
#include "Window.h"
#include "FrameTime.h"
#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"
#include <wrl.h>
#include <memory>
#include <Core/Layer.h>
#include <Core/LayerStack.h>
#include "renderer/Renderer.h"



namespace DXEngine {

	class Application
	{
	public:
		Application(HINSTANCE hInstance, int showWnd);
		virtual ~Application();

		int createLoop();
	private:
		void Render();
		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

		inline static  Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }


		// 2) Individual handlers for events you care about
		bool OnWindowResize(WindowResizeEvent& e);
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnMouseMoved(MouseMovedEvent& e);
		bool OnMouseScrolled(MouseScrolledEvent& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
		bool OnMouseButtonReleased(MouseButtonReleasedEvent& e);



	private:
		std::unique_ptr<Window> m_Window;
		bool m_Minimized = false;
		LayerStack m_LayerStack;

		static Application* s_Instance;

	};

	Application* CreateApplication();
}
