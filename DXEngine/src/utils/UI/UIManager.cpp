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

        Renderer::UpdateUIProjectionMatrix(screenWidth, screenHeight);

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
    void UIManager::UpdateElementRecursive(std::shared_ptr<UIElement> element, FrameTime dt)
    {
        if (!element)return;

        for (const auto& child : element->GetChildren())
        {
            if (child && child->IsEnabled())
            {
                UpdateElementRecursive(child, dt);
            }
        }
    }
    void UIManager::Render()
    {
        SubmitForRendering();

        if (mDX_DEBUGMode)
        {
            RenderDebugInfo();
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

        if (mDX_DEBUGMode)
        {
            RenderDebugInfo();
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
    void UIManager::SubmitDebugElements()
    {

        //debug UI elements such as ui to show mouse pos click etc

        if (mDX_DEBUGFrameCounter++ % 60 == 0) // Every second at 60fps
        {
            std::string debugMsg = "UI Debug - Elements: " + std::to_string(m_RootElements.size()) +
                ", Mouse: (" + std::to_string(m_LastMouseX) + ", " + std::to_string(m_LastMouseY) + ")\n";
            OutputDebugStringA(debugMsg.c_str());
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

        Renderer::UpdateUIProjectionMatrix(width, height);

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

        bool inputConsumed = false;

        // Process input for all elements (in reverse order for proper hit testing)
        // Later elements are drawn on top, so they should get input first
        for (auto it = m_RootElements.rbegin(); it != m_RootElements.rend(); ++it)
        {
            auto& element = *it;
            if (element && element->IsEnabled() && element->IsVisible())
            {
                if (HandleElementInputRecursive(element, mouseX, mouseY, leftClick, rightClick))
                {
                    inputConsumed = true;
                    break; // Input was consumed by this element
                }
            }
        }

        m_LastLeftClick = leftClick;
        m_LastRightClick = rightClick;
    }
    bool UIManager::HandleElementInputRecursive(std::shared_ptr<UIElement> element, float mouseX, float mouseY, bool leftClick, bool rightClick)
    {
        if (!element || !element->IsEnabled() || !element->IsVisible())
            return false;

        const auto& children = element->GetChildren();
        for (auto it = children.rbegin(); it != children.rend(); ++it)
        {
            if (HandleElementInputRecursive(*it, mouseX, mouseY, leftClick, rightClick))
            {
                return true;
            }
        }

        return element->HandleInput(mouseX, mouseY, leftClick, rightClick);
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
        AddElement(std::reinterpret_pointer_cast<UIElement>(textElement));
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
            if (element && element->IsVisible())
            {
                UIElement* found = FindElementAtRecursive(element.get(), x, y);
                if (found) return found;
            }
        }
        return nullptr;
    }
    UIElement* UIManager::FindElementAtRecursive(UIElement* element, float x, float y)
    {
        if (!element || !element->IsVisible()) return nullptr;

        const auto& children = element->GetChildren();
        for (auto it = children.rbegin(); it != children.rend(); ++it)
        {
            UIElement* found = FindElementAtRecursive(it->get(), x, y);
            if (found) return found;
        }

        //check this element
        if (element->GetBounds().Contains(x, y))
        {
            return element;
        }

        return nullptr;
    }
}