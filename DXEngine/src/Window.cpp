#include "dxpch.h"
#include "Window.h"
#include "Event/KeyEvent.h"
#include "windows/WindowsInput.h"
#include "Event/ApplicationEvent.h"
#include <windowsx.h>
#include "Event/MouseEvent.h"

namespace DXEngine {

#define BIND_EVENT_FN(x) std::bind(&Window::x, this, std::placeholders::_1)

	Window::Window(HINSTANCE hInstance, int nCmdShow, LPCWSTR windowTitle, LPCWSTR windowClass, int width, int height)
		:
		hInstance(hInstance),
		nShowWnd(nCmdShow),
		windowTitle(windowTitle),
		windowClass(windowClass),
		m_Width(width),
		m_Height(height)
	{

		if (!Initialize())
		{
			MessageBox(hwnd, L"failed to create Window", L"ERROR", MB_OK | MB_ICONERROR);
		}
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
			m_Width, m_Height,
			NULL,
			NULL,
			hInstance,
			this
		);

		if (!hwnd)
		{
			MessageBox(NULL, L"Error creating window",
				L"Error", MB_OK | MB_ICONERROR);
			return 1;
		}

		ShowWindow(hwnd, nShowWnd);
		UpdateWindow(hwnd);

		Input::Init(new WindowsInput(hwnd));


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
			// -- keyboard down --
		case WM_KEYDOWN:
		{
			bool repeat = (lParam & (1 << 30)) != 0;
			KeyPressedEvent e((int)wParam, repeat);
			if (m_EventCallback)
				m_EventCallback(e);
			return e.Handled ? 0 : DefWindowProc(hWnd, msg, wParam, lParam);
		}

		// -- keyboard up --
		case WM_KEYUP:
		{
			KeyReleasedEvent e((int)wParam);
			m_EventCallback(e);
			return e.Handled ? 0 : DefWindowProc(hWnd, msg, wParam, lParam);
		}
		case WM_SIZE:
		{
			int newW = LOWORD(lParam);
			int newH = HIWORD(lParam);

			// 1) Construct the event
			WindowResizeEvent e(newW, newH);

			// 2) Dispatch to whoever registered
			if (m_EventCallback)
				m_EventCallback(e);

			// 3) If handled, swallow it; otherwise pass on
			return e.Handled
				? 0
				: DefWindowProc(hWnd, msg, wParam, lParam);
		}
		case WM_MOUSEMOVE:
		{
			float x = static_cast<float>(GET_X_LPARAM(lParam));
			float y = static_cast<float>(GET_Y_LPARAM(lParam));
			MouseMovedEvent e(x, y);
			if (m_EventCallback)
				m_EventCallback(e);
			return e.Handled ? 0 : DefWindowProcW(hwnd, msg, wParam, lParam);
		}
		case WM_MOUSEWHEEL:
		{
			float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
			MouseScrolledEvent e(0.0f, delta);
			if (m_EventCallback) m_EventCallback(e);
			return e.Handled ? 0 : DefWindowProc(hWnd, msg, wParam, lParam);
		}
		case WM_LBUTTONDOWN:
		{
			MouseButtonPressedEvent e(VK_LBUTTON);
			if (m_EventCallback) m_EventCallback(e);
			return e.Handled ? 0 : DefWindowProc(hWnd, msg, wParam, lParam);
		}
		case WM_LBUTTONUP:
		{
			MouseButtonReleasedEvent e(VK_LBUTTON);
			if (m_EventCallback) m_EventCallback(e);
			return e.Handled ? 0 : DefWindowProc(hWnd, msg, wParam, lParam);
		}

		case WM_RBUTTONDOWN:
		{
			MouseButtonPressedEvent e(VK_RBUTTON);
			if (m_EventCallback) m_EventCallback(e);
			return e.Handled ? 0 : DefWindowProc(hWnd, msg, wParam, lParam);
		}
		case WM_RBUTTONUP:
		{
			MouseButtonReleasedEvent e(VK_RBUTTON);
			if (m_EventCallback) m_EventCallback(e);
			return e.Handled ? 0 : DefWindowProc(hWnd, msg, wParam, lParam);
		}
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
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

	void Window::SetEventCallback(const EventCallbackFn& callback)
	{
		m_EventCallback = callback;

	}

	void Window::QuitWindow()
	{
		DestroyWindow(hwnd);
	}
}
