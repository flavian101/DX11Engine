#pragma once
#include <Windows.h>
#include <optional>

class Window
{
private:
	HWND hwnd;
	HINSTANCE hInstance;
	int nShowWnd;
	LPCWSTR windowTitle;
	LPCWSTR windowClass;
	int width;
	int height;

public:
	Window(HINSTANCE hInstance, int nCmdShow,
		LPCWSTR windowTitle, LPCWSTR windowClass,
		int width, int height);
	~Window();
	bool Initialize();
	static std::optional<int> ProcessMessages();

	HWND GetHwnd() const { return hwnd; };
private:
	static LRESULT CALLBACK HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
	static LRESULT CALLBACK HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
	LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;



};

