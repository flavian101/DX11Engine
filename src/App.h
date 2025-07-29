#pragma once
#include "Window.h"
#include "FrameTime.h"
#include "Mesh.h"
#include "Triangle.h"
#include "Ball.h"
#include "SkySphere.h"
#include "Camera.h"
#pragma comment (lib, "dinput8.lib")
#pragma comment (lib, "dxguid.lib")
#include <dinput.h>
#include <wrl.h>
#include <memory>
#include "LightSphere.h"
#include "Event/ApplicationEvent.h"
#include "Event/KeyEvent.h"

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

	void DetectInput(double time);

private:
	Window window;
	FrameTime timer;
	std::shared_ptr<Camera> camera;
	Triangle tri;
	SkySphere sky;
	Ball ball;
	std::shared_ptr<LightSphere> m_Light;
	//Window wnd;
};

