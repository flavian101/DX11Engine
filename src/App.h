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

class App
{
public:
	App(HINSTANCE hInstance, int showWnd);
	~App();

	int createLoop();
private:
	void Render();
	void DetectInput(double time);
private:
	Window window;
	FrameTime timer;
	std::shared_ptr<Camera> camera;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> pKeyboard;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> pMouse;
	DIMOUSESTATE mouseLastState;
	LPDIRECTINPUT8 DirectInput;
	Triangle tri;
	SkySphere sky;
	Ball ball;
	std::shared_ptr<LightSphere> m_Light;
	//Window wnd;
};

