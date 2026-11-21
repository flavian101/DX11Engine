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

	protected:
		// ---- hooks for the application that hosts the engine (the editor, a game); the defaults do nothing ----
		// The UI of a frame is built between these two: after the scene was drawn (layers' OnUpdate), with every layer's
		// OnUIRender in between, and before the frame is presented.
		virtual void OnUIFrameBegin() {}
		virtual void OnUIFrameEnd() {}
		// First look at every event, before the layers see it (the window resize has been handled already). Set
		// e.Handled to keep the layers from seeing it as unhandled input; they still get to see it once.
		virtual void OnApplicationEvent(Event& e) { (void)e; }

	private:
		void Render();
		void OnEvent(Event& e);

		bool OnWindowResize(WindowResizeEvent& e);



	private:
		FrameTime m_Deltatime;
		std::unique_ptr<Window> m_Window;
		bool m_Minimized = false;
		LayerStack m_LayerStack;

		static Application* s_Instance;

	};

	Application* CreateApplication();
}
