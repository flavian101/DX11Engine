#pragma once
#include "dxpch.h"
#include "..\Core\Input.h"


namespace DXEngine {

	class WindowsInput : public Input
	{
	public:
		WindowsInput(HWND hwnd);
	protected:
		virtual bool IsKeyPressedImpl(int keycode) override;
		virtual bool IsButtonPressedImpl(int button) override;
		virtual std::pair<float, float> GetMousePositionImpl() override;
		virtual float GetMouseXImpl() override;
		virtual float GetMouseYImpl() override;
	private:
		HWND m_hwnd;
	};
}

