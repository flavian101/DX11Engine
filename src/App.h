#pragma once
#include "Window.h"
#include "FrameTime.h"
#include "utils\Mesh.h"
#include "Triangle.h"
#include "Ball.h"
#include "SkySphere.h"
#include "Camera.h"
#pragma comment (lib, "dxguid.lib")
#include <wrl.h>
#include <memory>
#include "LightSphere.h"
#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"
#include "Event/MouseEvent.h"
#include <shaders/ShaderManager.h>
#include "picking/PickingManager.h"

namespace DXEngine {
	class App
	{
	public:
		App(HINSTANCE hInstance, int showWnd);
		~App();

		int createLoop();
	private:
		void Render();
		void OnEvent(Event& e);

		// 2) Individual handlers for events you care about
		bool OnWindowResize(WindowResizeEvent& e);
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnMouseMoved(MouseMovedEvent& e);
		bool OnMouseScrolled(MouseScrolledEvent& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
		bool OnMouseButtonReleased(MouseButtonReleasedEvent& e);

		void DetectInput(double time);


		void HandlePicking(float mouseX, float mouseY);
		void InitializePicking();

	private:
		Window window;
		FrameTime timer;
		std::shared_ptr <ShaderManager> m_ShaderManager;
		std::shared_ptr<Camera> camera;
		std::shared_ptr<Triangle> tri;
		std::shared_ptr<SkySphere> sky;
		std::shared_ptr<Ball> ball;
		std::shared_ptr<LightSphere> m_Light;
		//Window wnd;
		std::unique_ptr<PickingManager> m_PickingManager;

		// Mouse state for picking
		float m_LastMouseX = 0.0f;
		float m_LastMouseY = 0.0f;

	};
}
