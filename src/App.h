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
class App
{
public:
	App(HINSTANCE hInstance, int showWnd);

	int createLoop();
private:
	void Render();
	void DetectInput(double time);
private:
	Microsoft::WRL::ComPtr<IDirectInputDevice8> pKeyboard;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> pMouse;
	DIMOUSESTATE mouseLastState;
	LPDIRECTINPUT8 DirectInput;
	Window window;
	FrameTime timer;
	Camera camera;
	Triangle tri;
	SkySphere sky;
	//Ball ball;
	//Window wnd;
};

