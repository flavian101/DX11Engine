#include "Window.h"



Window::Window(HINSTANCE hInstance, int nCmdShow, LPCWSTR windowTitle, LPCWSTR windowClass, int width, int height)
	:
	hInstance(hInstance),
	nShowWnd(nCmdShow),
	windowTitle(windowTitle),
	windowClass(windowClass),
	width(width),
	height(height)
{

	if (!Initialize())
	{
		MessageBox(hwnd,L"failed to create Window", L"ERROR", MB_OK | MB_ICONERROR);
	}
}

Graphics& Window::Gfx()
{
	if (!pGfx)
	{
		MessageBox(hwnd, L"failed initialize graphics Object",L"error",MB_OK);
	}
	return *pGfx;
}

HWND Window::GetHwnd() const
{
	return hwnd;
}

Window::~Window()
{
	DestroyWindow(hwnd);
}

bool Window::Initialize()
{
	typedef struct _WNDCLASS {
		UINT cbSize;
		UINT style;
		WNDPROC lpfnWndProc;
		int cbClsExtra;
		int cbWndExtra;
		HANDLE hInstance;
		HICON hIcon;
		HCURSOR hCursor;
		HBRUSH hbrBackground;
		LPCTSTR lpszMenuName;
		LPCTSTR lpszClassName;
	} WNDCLASS;

	WNDCLASSEX wc = {};

	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = HandleMsgSetup;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = static_cast<HICON>(LoadImage(
		hInstance, MAKEINTRESOURCE(IDI_ICON2),
		IMAGE_ICON, 256, 201, 0
	));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	
	wc.hbrBackground = nullptr;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = windowClass;
	wc.hIconSm = static_cast<HICON>(LoadImage(
		hInstance, MAKEINTRESOURCE(IDI_ICON2),
		IMAGE_ICON, 256, 201, 0
	));


	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, L"Error registering class",
			L"Error", MB_OK | MB_ICONERROR);
		return 1;
	}


	hwnd = CreateWindowEx(
		NULL,
		windowClass,
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (!hwnd)
	{
		MessageBox(NULL, L"Error creating window",
			L"Error", MB_OK | MB_ICONERROR);
		return 1;
	}

	ShowWindow(hwnd, nShowWnd);
	UpdateWindow(hwnd);

	// create graphics object
	pGfx = std::make_unique<Graphics>(hwnd, width, height,false);

	return true;
}

LRESULT Window::HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	// use create parameter passed in from CreateWindow() to store window class pointer at WinAPI side
	if (msg == WM_NCCREATE)
	{
		// extract ptr to window class from creation data
		const CREATESTRUCTW* const pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
		Window* const pWnd = static_cast<Window*>(pCreate->lpCreateParams);
		// set WinAPI-managed user data to store ptr to window instance
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
		// set message proc to normal (non-setup) handler now that setup is finished
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Window::HandleMsgThunk));
		// forward message to window instance handler
		return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
	}
	// if we get a message before the WM_NCCREATE message, handle with default handler
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT Window::HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	// retrieve ptr to window instance
	Window* const pWnd = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	// forward message to window instance handler
	return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
}

LRESULT Window::HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			DestroyWindow(hWnd);
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

std::optional<int> Window::ProcessMessages()
{
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			return (int)msg.wParam;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		
	}
	return {};
}
