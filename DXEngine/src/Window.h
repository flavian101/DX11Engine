#pragma once
#include "Event/Event.h"
#include <optional>

namespace DXEngine {

	class Window
	{

	public:
		using EventCallbackFn = std::function<void(Event&)>;
		// Sees EVERY native message before the engine does; return true to say "consumed, do not handle it again".
		// (The ImGui layer needs the raw messages: mouse, keyboard, text, cursor.)
		using NativeMessageHook = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;

		Window(HINSTANCE hInstance, int nCmdShow,
			LPCWSTR windowTitle, LPCWSTR windowClass,
			int width, int height);
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;
		~Window();
		bool Initialize();
		static std::optional<int> ProcessMessages();
		void SetEventCallback(const EventCallbackFn& callback);
		void SetNativeMessageHook(NativeMessageHook hook) { m_NativeMessageHook = std::move(hook); }
		void QuitWindow();
		void CaptureCursor();
		void ReleaseCursor();
		bool IsCursorCaptured() const { return m_IsCursorCaptured; }

		HWND GetHwnd() const;
		int GetWidth() const { return m_Width; }
		int GetHeight() const { return m_Height; }
	private:

		static LRESULT CALLBACK HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
		static LRESULT CALLBACK HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;
		LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

	private:
		HWND hwnd;
		HINSTANCE hInstance;
		int nShowWnd;
		LPCWSTR windowTitle;
		LPCWSTR windowClass;
		int m_Width;
		int m_Height;
		EventCallbackFn m_EventCallback;
		POINT m_SavedCursor{ 0, 0 };         // where the cursor was when it was captured (screen coordinates)
		NativeMessageHook m_NativeMessageHook;
		bool m_IsCursorCaptured;

	};

}