#include "dxpch.h"
#include "UIText.h"

namespace DXEngine
{
    UIText::UIText(const std::string& text, const UIRect& bounds)
        : UIElement(bounds), m_Text(text)
    {
    }

    void UIText::Update(FrameTime dt)
    {
        // Text elements typically don't need complex updates
        // Could add text animations, typewriter effects, etc. here
    }

    void UIText::Render()
    {
        if (!IsVisible()) return;

        const UIRect& bounds = GetBounds();

        // This is where you'd use your UI renderer to draw the text
        // For now, we'll use placeholder rendering

        // Calculate text position based on alignment
        UIRect textRect = bounds;
        switch (m_Alignment)
        {
        case TextAlignment::Center:
            // Center text horizontally
            break;
        case TextAlignment::Right:
            // Right-align text
            break;
        case TextAlignment::Left:
        default:
            // Left-align text (default)
            break;
        }

        // Draw text (placeholder - you'd use your UIRenderer here)
        // UIRenderer::DrawText(m_Text, textRect, m_Color, m_FontSize);

        // For debugging, output occasionally
        static int frameCount = 0;
        if (frameCount++ % 120 == 0 && IsVisible()) // Every 120 frames (2 seconds at 60fps)
        {
            std::string debugMsg = "Rendering text: \"" + m_Text +
                "\" at (" + std::to_string(bounds.x) + "," + std::to_string(bounds.y) + ")\n";
            OutputDebugStringA(debugMsg.c_str());
        }
    }

    bool UIText::HandleInput(float mouseX, float mouseY, bool leftClick, bool rightClick)
    {
        // Basic text elements don't handle input
        // Could be extended for selectable text, hyperlinks, etc.
        return false;
    }
}