#include "WindowsInput.h"


WindowsInput::WindowsInput(HWND hwnd)
:
	m_hwnd(hwnd)
{
}

bool WindowsInput::IsKeyPressedImpl(int keycode)
{
	return (GetAsyncKeyState(keycode) & 0x8000) != 0;
}

bool WindowsInput::IsButtonPressedImpl(int button)
{
	return (GetAsyncKeyState(button) & 0x8000) != 0;
}

std::pair<float, float> WindowsInput::GetMousePositionImpl()
{
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(m_hwnd, &p);
	return { (float)p.x, (float)p.y };
}

float WindowsInput::GetMouseXImpl()
{
	return GetMousePositionImpl().first;
}

float WindowsInput::GetMouseYImpl()
{
	return GetMousePositionImpl().second;
}
