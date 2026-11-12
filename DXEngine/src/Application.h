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
#include "Core/ImGuiLayer.h"



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
		inline static bool HasInstance() { return s_Instance != nullptr; }
		inline Window& GetWindow() { return *m_Window; }
		inline ImGuiLayer& GetImGuiLayer() { return *m_ImGuiLayer; }

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
		ImGuiLayer* m_ImGuiLayer;
		bool m_Minimized = false;
		LayerStack m_LayerStack;

		static Application* s_Instance;

	};

	Application* CreateApplication();
}
