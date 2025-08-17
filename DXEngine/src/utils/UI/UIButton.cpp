#include "dxpch.h"
#include "UIButton.h"


namespace DXEngine
{
	UIButton::UIButton(const std::string& text, const UIRect& bounds)
        :
        UIElement(bounds),
        m_Text(text)
	{
	}
	void UIButton::Update(FrameTime dt)
	{
		// Update any animations or time-based effects here
		// For now, basic button doesn't need complex updates
	}
	
	bool UIButton::HandleInput(float mouseX, float mouseY, bool leftClick, bool rightClick)
	{
        if (!IsEnabled() || !IsVisible() || !m_Interactable)
            return false;

        const UIRect& bounds = GetBounds();
        bool isMouseInside = bounds.Contains(mouseX, mouseY);

        // Update button state
        ButtonState previousState = m_State;
        UpdateState(mouseX, mouseY, leftClick);

        // Handle click events
        if (isMouseInside && leftClick && !m_WasPressed && m_OnClick)
        {
            m_OnClick();

            // Debug output for click
            OutputDebugStringA(("Button '" + m_Text + "' clicked!\n").c_str());

            return true; // Consume the input
        }

        m_WasPressed = leftClick && isMouseInside;

        // Return true if mouse is over button to consume input
        return isMouseInside;
    }

    void UIButton::UpdateState(float mouseX, float mouseY, bool leftClick)
    {
        if (!m_Interactable)
        {
            m_State = ButtonState::Disabled;
            return;
        }

        const UIRect& bounds = GetBounds();
        bool isMouseInside = bounds.Contains(mouseX, mouseY);

        if (!isMouseInside)
        {
            m_State = ButtonState::Normal;
        }
        else if (leftClick)
        {
            m_State = ButtonState::Pressed;
        }
        else
        {
            m_State = ButtonState::Hovered;
        }
    }
    UIColor UIButton::GetCurrentColor() const
    {
        switch (m_State)
        {
        case ButtonState::Hovered:
            return m_HoverColor;
        case ButtonState::Pressed:
            return m_PressedColor;
        case ButtonState::Disabled:
            return UIColor(m_NormalColor.r * 0.5f, m_NormalColor.g * 0.5f, m_NormalColor.b * 0.5f, m_NormalColor.a);
        case ButtonState::Normal:
        default:
            return m_NormalColor;
        }
    
    }
}