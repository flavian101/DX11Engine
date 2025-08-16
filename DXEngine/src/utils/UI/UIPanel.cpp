#include "dxpch.h"
#include "UIPanel.h"

namespace DXEngine
{
    UIPanel::UIPanel(const UIRect& bounds, const UIColor& backgroundColor)
        :
        UIElement(bounds), m_BackgroundColor(backgroundColor)
    { }

    void UIPanel::Update(FrameTime dt)
    {
        //update all children
        for (auto& child : GetChildren())
        {
            if (child && child->IsEnabled())
            {
                child->Update(dt);
            }
        }
    }
  
    bool UIPanel::HandleInput(float mouseX, float mouseY, bool leftClick, bool rightClick)
    {
        if (!IsEnabled() || !IsVisible())
            return false;

        const UIRect& bounds = GetBounds();
        bool isMouseInside = bounds.Contains(mouseX, mouseY);

        // Check children first (reverse order for proper hit testing)
        const auto& children = GetChildren();
        for (auto it = children.rbegin(); it != children.rend(); ++it)
        {
            auto& child = *it;
            if (child && child->IsEnabled() && child->IsVisible())
            {
                if (child->HandleInput(mouseX, mouseY, leftClick, rightClick))
                {
                    return true; // Input consumed by child
                }
            }
        }

        // If mouse is inside panel bounds, consume the input to prevent elements behind from receiving it
        return isMouseInside;
    }

    void UIPanel::AutoSizeToFitChildren()
    {
        if (GetChildren().empty())
            return;

        const UIRect& currentBounds = GetBounds();
        float minX = currentBounds.x;
        float minY = currentBounds.y;
        float maxX = currentBounds.x;
        float maxY = currentBounds.y;

        for (const auto& child : GetChildren())
        {
            if (child)
            {
                const UIRect& childBounds = child->GetBounds();
                minX = std::min(minX, childBounds.x);
                minY = std::min(minY, childBounds.y);
                maxX = std::max(maxX, childBounds.x + childBounds.width);
                maxY = std::max(maxY, childBounds.y + childBounds.height);
            }
        }

        // Add some padding
        const float padding = 10.0f;
        UIRect newBounds(
            minX - padding,
            minY - padding,
            (maxX - minX) + (padding * 2),
            (maxY - minY) + (padding * 2)
        );

        setBounds(newBounds);
    }

    void UIPanel::ArrangeChildrenVertically(float spacing)
    {
        const UIRect& bounds = GetBounds();
        float currentY = bounds.y + 10.0f; // Start with some padding from top

        for (auto& child : GetChildren())
        {
            if (child && child->IsVisible())
            {
                UIRect childBounds = child->GetBounds();
                childBounds.y = currentY;
                childBounds.x = bounds.x + 10.0f; // Some padding from left
                child->setBounds(childBounds);

                currentY += childBounds.height + spacing;
            }
        }
    }

    void UIPanel::ArrangeChildrenHorizontally(float spacing)
    {
        const UIRect& bounds = GetBounds();
        float currentX = bounds.x + 10.0f; // Start with some padding from left

        for (auto& child : GetChildren())
        {
            if (child && child->IsVisible())
            {
                UIRect childBounds = child->GetBounds();
                childBounds.x = currentX;
                childBounds.y = bounds.y + 10.0f; // Some padding from top
                child->setBounds(childBounds);

                currentX += childBounds.width + spacing;
            }
        }
    }
}