#pragma once
#include "Window.h"
#include "FrameTime.h"
#include "Mesh.h"
#include "Triangle.h"
#include "Camera.h"

class App
{
public:
	App(HINSTANCE hInstance, int showWnd);

	int createLoop();
private:
	void Render();
private:
	Window window;
	FrameTime timer;
	Camera camera;
	Triangle tri;
	//Window wnd;
};

