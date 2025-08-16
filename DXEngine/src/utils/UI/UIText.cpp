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

    bool UIText::HandleInput(float mouseX, float mouseY, bool leftClick, bool rightClick)
    {
        // Basic text elements don't handle input
        // Could be extended for selectable text, hyperlinks, etc.
        return false;
    }
}