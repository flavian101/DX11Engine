#pragma once
#include "Graphics.h"
#include "resource.h"
#include <Windows.h>
#include <optional>
#include <memory>

class Window
{
	friend class Graphics;
public:
	Window(HINSTANCE hInstance, int nCmdShow,
		LPCWSTR windowTitle, LPCWSTR windowClass,
		int width, int height);
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	~Window();
	bool Initialize();
	static std::optional<int> ProcessMessages();
	Graphics& Gfx();

	HWND GetHwnd() const { return hwnd; };
private:
	
	static LRESULT CALLBACK HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
	static LRESULT CALLBACK HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
	LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
	
private:
	HWND hwnd;
	std::unique_ptr<Graphics> pGfx;
	HINSTANCE hInstance;
	int nShowWnd;
	LPCWSTR windowTitle;
	LPCWSTR windowClass;
	int width;
	int height;

	

};

