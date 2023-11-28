#pragma once
#include "Window.h"
#include "FrameTime.h"


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
	//Window wnd;
};

