#pragma once
#include "dxpch.h"
#include "Window.h"
#include "FrameTime.h"
#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"
#include <Core/Layer.h>
#include <Core/LayerStack.h>
#include "renderer/Renderer.h"
#include "utils/UI/UILayer.h"



namespace DXEngine {

	class Application
	{
	public:
		Application();


		virtual ~Application();

		int createLoop();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

		inline static  Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }

	private:
		void Render();
		void OnEvent(Event& e);

		// 2) Individual handlers for events you care about
		bool OnWindowResize(WindowResizeEvent& e);
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnMouseButtonPressedEvent(MouseButtonPressedEvent& event);
		bool OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& event);



	private:
		FrameTime m_Deltatime;
		std::unique_ptr<Window> m_Window;
		UILayer* m_UILayer;
		bool m_Minimized = false;
		LayerStack m_LayerStack;

		static Application* s_Instance;

	};

	Application* CreateApplication();
}
