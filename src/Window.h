#pragma once
#define NOMINMAX 
#include "renderer/RendererCommand.h"
#include "resource.h"
#include <Windows.h>
#include <optional>
#include <memory>
#include "Event/Event.h"
#include <functional>

namespace DXEngine {

	class Window
	{

		friend class Graphics;
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		Window(HINSTANCE hInstance, int nCmdShow,
			LPCWSTR windowTitle, LPCWSTR windowClass,
			int width, int height);
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;
		~Window();
		bool Initialize();
		static std::optional<int> ProcessMessages();
		void SetEventCallback(const EventCallbackFn& callback);
		void QuitWindow();

		HWND GetHwnd() const;
		int GetWidth() const { return m_Width; }
		int GetHeight() const { return m_Height; }
	private:

		static LRESULT CALLBACK HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
		static LRESULT CALLBACK HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
		LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

	private:
		std::unique_ptr<Graphics> p;
		HWND hwnd;
		HINSTANCE hInstance;
		int nShowWnd;
		LPCWSTR windowTitle;
		LPCWSTR windowClass;
		int m_Width;
		int m_Height;
		EventCallbackFn m_EventCallback;

	};

}