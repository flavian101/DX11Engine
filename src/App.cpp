#include "App.h"
#include <optional>

App::App(HINSTANCE hInstance, int showWnd)
    :
    window(hInstance, showWnd, L"engine", L"DirectX", 1270, 720),
    tri(window.Gfx()),
	//ball(window.Gfx()),
	sky(window.Gfx())
{

    window.Gfx().SetProjection(DirectX::XMMatrixPerspectiveLH(1.0f, 9.0f / 16.0f, 0.5f, 100.0f));
   // m(window.Gfx());

	HRESULT hr;
	hr = DirectInput8Create(hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)&DirectInput,
		NULL);

	hr = DirectInput->CreateDevice(GUID_SysKeyboard,
		&pKeyboard,
		NULL);

	hr = DirectInput->CreateDevice(GUID_SysMouse,
		&pMouse,
		NULL);

	hr = pKeyboard->SetDataFormat(&c_dfDIKeyboard);
	hr = pKeyboard->SetCooperativeLevel(window.Gfx().getHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

	hr = pMouse->SetDataFormat(&c_dfDIMouse);
	hr = pMouse->SetCooperativeLevel(window.Gfx().getHwnd(), DISCL_EXCLUSIVE | DISCL_NOWINKEY | DISCL_FOREGROUND);
	//hr = pMouse->SetCooperativeLevel(window.Gfx().getHwnd(), DISCL_NOWINKEY | DISCL_FOREGROUND);

	
}

int App::createLoop()
{
    while (true)
    {
        //process all messages pending 
        if (const auto ecode = Window::ProcessMessages())
        {
            //if return optional has value, means we'are exiting the program by returning the exit code
            return *ecode;
        }
        Render();
	}


	
}

void App::Render()
{
    const float t = timer.Peek();
    window.Gfx().ClearDepthColor(0.1f, 0.1f, 0.16f);
	DetectInput(t);

    window.Gfx().SetCamera(camera.GetView());
	
	sky.Draw(window.Gfx(), camera.GetPos(), camera.GetTarget());
    tri.Draw(window.Gfx(),camera.GetPos(),camera.GetTarget());
	//ball.Draw(window.Gfx(), camera.GetPos(), camera.GetTarget());
	
    window.Gfx().End();
}

void App::DetectInput(double time)
{
	DIMOUSESTATE mouseCurrState;

	BYTE keyboardState[256];

	pKeyboard->Acquire();
	pMouse->Acquire();

	pMouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseCurrState);

	pKeyboard->GetDeviceState(sizeof(keyboardState), (LPVOID)&keyboardState);

	if (keyboardState[DIK_ESCAPE] & 0x80)
	{
		PostMessage(window.GetHwnd(), WM_DESTROY, 0, 0);
	}

	float speed = 0.02f * time;

	if (keyboardState[DIK_A] & 0x80)
	{
		camera.moveLeftRight -= speed;
	}
	else if (keyboardState[DIK_D] & 0x80)
	{
		camera.moveLeftRight += speed;
	}
	else if (keyboardState[DIK_W] & 0x80)
	{
		camera.moveBackForward += speed;
	}
	else if (keyboardState[DIK_S] & 0x80)
	{
		camera.moveBackForward -= speed;
	}
	else
	{
		speed = 0.02;
	}
	if ((mouseCurrState.lX != mouseLastState.lX) || (mouseCurrState.lY != mouseLastState.lY))
	{
		camera.camYaw += mouseLastState.lX * 0.001f;

		camera.camPitch += mouseCurrState.lY * 0.001f;

		mouseLastState = mouseCurrState;
	}
	camera.UpdateCamera();
	//call update

	return;
}
