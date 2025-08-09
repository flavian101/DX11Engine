#include "dxpch.h"
#include "UIButton.h"


namespace DXEngine
{
	UIButton::UIButton(const std::string& text, const UIRect& bounds)
	{
	}
	void UIButton::Update(FrameTime dt)
	{
		// Update any animations or time-based effects here
		// For now, basic button doesn't need complex updates
	}
	void UIButton::Render()
	{
        if (!IsVisible()) return;

        // Get current color based on state
        UIColor currentColor = GetCurrentColor();

        // This is where you'd use your UI renderer to draw the button
        // For now, we'll use placeholder rendering

        const UIRect& bounds = GetBounds();

        // Draw button background (placeholder - you'd use your UIRenderer here)
        // UIRenderer::DrawRect(bounds, currentColor);

        // Draw button border if needed
        // UIRenderer::DrawRectOutline(bounds, UIColor(0.6f, 0.6f, 0.6f, 1.0f), 1.0f);

        // Draw button text (placeholder - you'd use your UIRenderer here)
        // UIRenderer::DrawText(m_Text, bounds, m_TextColor, 16.0f);

        // For debugging, output to console
        static int frameCount = 0;
        if (frameCount++ % 60 == 0 && IsVisible()) // Only every 60 frames to avoid spam
        {
            std::string debugMsg = "Rendering button: " + m_Text +
                " at (" + std::to_string(bounds.x) + "," + std::to_string(bounds.y) +
                ") size (" + std::to_string(bounds.width) + "x" + std::to_string(bounds.height) + ")\n";
            OutputDebugStringA(debugMsg.c_str());
        }
	}
	bool UIButton::HandleInput(float mouseX, float mouseY, bool leftClick, bool rightClick)
	{
        if (!IsEnabled() || !IsVisible() || !m_Interactable)
            return false;

        const UIRect& bounds = GetBounds();
        bool isMouseInside = bounds.Contains(mouseX, mouseY);

        UpdateState(mouseX, mouseY, leftClick);

        // Handle click
        if (isMouseInside && leftClick && !m_WasPressed && m_OnClick)
        {
            m_OnClick();
            return true; // Consume the input
        }

        m_WasPressed = leftClick && isMouseInside;

        return isMouseInside; // Return true if mouse is over button to consume input
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