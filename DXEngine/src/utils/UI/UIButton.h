#pragma once
#include "UIElement.h"
#include <functional>
#include <string>


namespace DXEngine
{
	enum class ButtonState
	{
		Normal,
		Hovered,
		Pressed,
		Disabled
	};

	class UIButton: public UIElement
	{
	public:
		using ClickCallback = std::function<void()>;
		
		UIButton(const std::string& text, const UIRect& bounds);
		virtual ~UIButton() = default;

		//element interface
		virtual void Update(FrameTime dt) override;
		virtual void Render() override;
		virtual bool HandleInput(float mouseX, float mouseY, bool leftClick, bool rightClick) override;

		//button
		void SetText(const std::string& text) { m_Text = text; }
		const std::string& GetText() const { return m_Text; }

		void SetOnClick(ClickCallback callback) { m_OnClick = callback; }

		//styling
		void SetNormalColor(const UIColor& color) { m_NormalColor = color; }
		void SetHoverColor(const UIColor& color) { m_HoverColor = color; }
		void SetPressedColor(const UIColor& color) { m_PressedColor = color; }
		void SetTextColor(const UIColor& color) { m_TextColor = color; }
		
		UIColor GetHoverColor() { return m_HoverColor; }
		UIColor GetNormalColor() { return m_NormalColor; }
		UIColor GetPressedColor(){ return m_PressedColor; }
		UIColor GetTextColor() { return m_TextColor; }


		//state of the button
		ButtonState GetState() const { return m_State; }
		void SetInteractable(bool interactable) { m_Interactable = interactable; }
		bool IsInteractable() const { return m_Interactable; }

	private:
		void UpdateState(float mouseX, float mouseY, bool leftClick);
		UIColor GetCurrentColor() const;


	private:
		std::string m_Text;
		ClickCallback m_OnClick;
		UIColor m_NormalColor = UIColor(0.3f, 0.3f, 0.3f, 1.0f);
		UIColor m_HoverColor = UIColor(0.4f, 0.4f, 0.4f, 1.0f);
		UIColor m_PressedColor = UIColor(0.2f, 0.2f, 0.2f, 1.0f);
		UIColor m_TextColor = UIColor(1.0f, 1.0f, 1.0f, 1.0f);

		ButtonState m_State = ButtonState::Normal;
		bool m_Interactable = true;
		bool m_WasPressed = false;

	};
}
