#pragma once
#include "utils/UI/UIElement.h"
#include "utils/UI/UIButton.h"
#include "utils/UI/UIText.h"
#include "utils/UI/UIPanel.h"
#include "renderer/Renderer.h"
#include "Event/Event.h"
#include <memory>
#include <vector>

namespace DXEngine
{
    class UIManager
    {
    public:
        UIManager();
        ~UIManager();

        void Initialize(int screenWidth, int screenHeight);
        void Shutdown();

        // Update and render - now submits to unified renderer
        void Update(FrameTime dt);
        void Render();
        void SubmitForRendering(); 

        // Event handling
        void OnEvent(Event& event);

        // UI element management
        void AddElement(std::shared_ptr<UIElement> element);
        void RemoveElement(std::shared_ptr<UIElement> element);
        void ClearElements();

        // Screen management
        void SetScreenSize(int width, int height);
        void GetScreenSize(int& width, int& height) const;

        // Input handling
        void HandleMouseInput(float mouseX, float mouseY, bool leftClick, bool rightClick);

        // Factory methods
        std::shared_ptr<UIButton> CreateButton(const std::string& text, const UIRect& bounds);
        std::shared_ptr<UIText> CreateText(const std::string& text, const UIRect& bounds);
        std::shared_ptr<UIPanel> CreatePanel(const UIRect& bounds, const UIColor& color = UIColor(0.2f, 0.2f, 0.2f, 0.8f));

        // Style management
        void SetDefaultButtonStyle(const UIColor& normal, const UIColor& hover, const UIColor& pressed);

        // Debug
        void SetDebugMode(bool enabled) { mDX_DEBUGMode = enabled; }
        bool IsDebugMode() const { return mDX_DEBUGMode; }

        UIElement* FindElementAt(float x, float y);

    private:
        void SubmitElementForRendering(std::shared_ptr<UIElement> element);
        void SubmitDebugElements();
        void RenderDebugInfo();

        void UpdateElementRecursive(std::shared_ptr<UIElement> element,FrameTime dt);

        bool HandleElementInputRecursive(std::shared_ptr <UIElement> element, float mouseX, float mouseY, bool leftClick, bool rightClick);
        UIElement* FindElementAtRecursive(UIElement* element, float x, float y);


    private:
        std::vector<std::shared_ptr<UIElement>> m_RootElements;
        int m_ScreenWidth = 1920;
        int m_ScreenHeight = 1080;
        bool mDX_DEBUGMode = false;
        uint32_t mDX_DEBUGFrameCounter = 0;

        // Style defaults
        UIColor m_DefaultButtonNormal = UIColor(0.3f, 0.3f, 0.3f, 1.0f);
        UIColor m_DefaultButtonHover = UIColor(0.4f, 0.4f, 0.4f, 1.0f);
        UIColor m_DefaultButtonPressed = UIColor(0.2f, 0.2f, 0.2f, 1.0f);

        // Mouse state
        float m_LastMouseX = 0.0f;
        float m_LastMouseY = 0.0f;
        bool m_LastLeftClick = false;
        bool m_LastRightClick = false;
    };
}
