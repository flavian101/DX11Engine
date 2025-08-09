#include "dxpch.h"
#include "utils/UI/UIManager.h"
#include "Event/MouseEvent.h"
#include "Event/KeyEvent.h"
#include "Event/ApplicationEvent.h"

namespace DXEngine
{
    UIManager::UIManager()
    {
    }

    UIManager::~UIManager()
    {
        Shutdown();
    }

    void UIManager::Initialize(int screenWidth, int screenHeight)
    {
        m_ScreenWidth = screenWidth;
        m_ScreenHeight = screenHeight;

        // Initialize UI renderer if you have one
        // m_UIRenderer.Initialize(screenWidth, screenHeight);
    }

    void UIManager::Shutdown()
    {
        ClearElements();
    }

    void UIManager::Update(FrameTime dt)
    {
        // Update all root elements (they will update their children)
        for (auto& element : m_RootElements)
        {
            if (element && element->IsEnabled())
            {
                element->Update(dt);
            }
        }
    }

    void UIManager::SubmitForRendering()
    {
        // Submit all visible root elements to the unified renderer
        for (auto& element : m_RootElements)
        {
            if (element && element->IsVisible())
            {
                SubmitElementForRendering(element);
            }
        }

        if (m_DebugMode)
        {
            // Submit debug UI elements
            // Could create debug panels and submit them too
        }
    }

    void UIManager::SubmitElementForRendering(std::shared_ptr<UIElement> element)
    {
        if (!element || !element->IsVisible())
            return;

        // Submit this element to the unified renderer
        Renderer::SubmitUI(element);

        // Recursively submit children
        for (auto& child : element->GetChildren())
        {
            SubmitElementForRendering(child);
        }
    }
}


    void UIManager::OnEvent(Event& event)
    {
        // Handle different event types
        if (event.GetEventType() == EventType::MouseButtonPressed ||
            event.GetEventType() == EventType::MouseButtonReleased ||
            event.GetEventType() == EventType::MouseMoved)
        {
            // Extract mouse position and button states from event
            // This is a simplified version - you'd need to properly extract from your Event system
            HandleMouseInput(m_LastMouseX, m_LastMouseY, m_LastLeftClick, m_LastRightClick);
        }

        if (event.GetEventType() == EventType::WindowResize)
        {
            // Handle window resize
            // WindowResizeEvent& resizeEvent = static_cast<WindowResizeEvent&>(event);
            // SetScreenSize(resizeEvent.GetWidth(), resizeEvent.GetHeight());
        }
    }

    void UIManager::AddElement(std::shared_ptr<UIElement> element)
    {
        if (element)
        {
            m_RootElements.push_back(element);
        }
    }

    void UIManager::RemoveElement(std::shared_ptr<UIElement> element)
    {
        if (element)
        {
            auto it = std::find(m_RootElements.begin(), m_RootElements.end(), element);
            if (it != m_RootElements.end())
            {
                m_RootElements.erase(it);
            }
        }
    }

    void UIManager::ClearElements()
    {
        m_RootElements.clear();
    }

    void UIManager::SetScreenSize(int width, int height)
    {
        m_ScreenWidth = width;
        m_ScreenHeight = height;

        // Update UI renderer if you have one
        // m_UIRenderer.SetScreenSize(width, height);
    }

    void UIManager::GetScreenSize(int& width, int& height) const
    {
        width = m_ScreenWidth;
        height = m_ScreenHeight;
    }

    void UIManager::HandleMouseInput(float mouseX, float mouseY, bool leftClick, bool rightClick)
    {
        m_LastMouseX = mouseX;
        m_LastMouseY = mouseY;

        // Process input for all elements (in reverse order for proper hit testing)
        for (auto it = m_RootElements.rbegin(); it != m_RootElements.rend(); ++it)
        {
            auto& element = *it;
            if (element && element->IsEnabled() && element->IsVisible())
            {
                if (element->HandleInput(mouseX, mouseY, leftClick, rightClick))
                {
                    // Input was consumed by this element
                    break;
                }
            }
        }

        m_LastLeftClick = leftClick;
        m_LastRightClick = rightClick;
    }

    std::shared_ptr<UIButton> UIManager::CreateButton(const std::string& text, const UIRect& bounds)
    {
        auto button = std::make_shared<UIButton>(text, bounds);

        // Apply default styling
        button->SetNormalColor(m_DefaultButtonNormal);
        button->SetHoverColor(m_DefaultButtonHover);
        button->SetPressedColor(m_DefaultButtonPressed);

        AddElement(button);
        return button;
    }

    std::shared_ptr<UIText> UIManager::CreateText(const std::string& text, const UIRect& bounds)
    {
        auto textElement = std::make_shared<UIText>(text, bounds);
        AddElement(std::dynamic_pointer_cast<UIElement>(textElement));
        return textElement;
    }

    std::shared_ptr<UIPanel> UIManager::CreatePanel(const UIRect& bounds, const UIColor& color)
    {
        auto panel = std::make_shared<UIPanel>(bounds, color);
        AddElement(panel);
        return panel;
    }

    void UIManager::SetDefaultButtonStyle(const UIColor& normal, const UIColor& hover, const UIColor& pressed)
    {
        m_DefaultButtonNormal = normal;
        m_DefaultButtonHover = hover;
        m_DefaultButtonPressed = pressed;
    }

    void UIManager::RenderDebugInfo()
    {
        // Render debug information like element bounds, hierarchy, etc.
        // This would use your UI renderer to draw debug overlays

        // Example debug info that would be rendered:
        std::string debugInfo = "UI Debug Info:\n";
        debugInfo += "Elements: " + std::to_string(m_RootElements.size()) + "\n";
        debugInfo += "Screen: " + std::to_string(m_ScreenWidth) + "x" + std::to_string(m_ScreenHeight) + "\n";
        debugInfo += "Mouse: (" + std::to_string(m_LastMouseX) + ", " + std::to_string(m_LastMouseY) + ")\n";

        // You would render this text in the top-left corner or similar
        OutputDebugStringA(debugInfo.c_str());
    }

    UIElement* UIManager::FindElementAt(float x, float y)
    {
        // Find the topmost element at the given coordinates
        for (auto it = m_RootElements.rbegin(); it != m_RootElements.rend(); ++it)
        {
            auto& element = *it;
            if (element && element->IsVisible() && element->GetBounds().Contains(x, y))
            {
                return element.get();
            }
        }
        return nullptr;
    }
}