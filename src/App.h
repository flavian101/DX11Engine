#pragma once
#include "Window.h"
#include "FrameTime.h"
#include "Mesh.h"
#include "Triangle.h"

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
	Triangle tri;
	//Window wnd;
};

