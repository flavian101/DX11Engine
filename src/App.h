#pragma once
#include "Window.h"


class App
{
public:
	App(HINSTANCE hInstance, int showWnd);

	int createLoop();
private:
	void Render();
private:
	Window window;
	Window wnd;
};

