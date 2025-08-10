#include "dxpch.h"
#include "utils/UI/UILayer.h"
#include "Core/Input.h"

namespace DXEngine
{
    UILayer::UILayer(const std::string& name)
        : Layer(name)
    {
        m_UIManager = std::make_unique<UIManager>();
    }

    UILayer::~UILayer()
    {
    }

    void UILayer::OnAttach()
    {
        // Initialize UI system with default screen size
        m_UIManager->Initialize(1920, 1080);

        // Create some example UI elements for testing
        CreateExampleUI();

        OutputDebugStringA("UILayer attached successfully\n");
    }

    void UILayer::OnDetach()
    {
        m_UIManager->Shutdown();
        OutputDebugStringA("UILayer detached\n");
    }

    void UILayer::OnUpdate(FrameTime dt)
    {
        // Handle input
        HandleInput();

        // Update UI
        m_UIManager->Update(dt);
    }

    void UILayer::OnUIRender()
    {
        // Render UI
        m_UIManager->Render();
    }

    void UILayer::OnEvent(Event& event)
    {
        m_UIManager->OnEvent(event);

        // Handle UI-specific events here if needed
        // For example, toggle debug mode with a key press
    }

    std::shared_ptr<UIButton> UILayer::CreateButton(const std::string& text, const UIRect& bounds)
    {
        return m_UIManager->CreateButton(text, bounds);
    }

    std::shared_ptr<UIText> UILayer::CreateText(const std::string& text, const UIRect& bounds)
    {
        return m_UIManager->CreateText(text, bounds);
    }

    std::shared_ptr<UIPanel> UILayer::CreatePanel(const UIRect& bounds, const UIColor& color)
    {
        return m_UIManager->CreatePanel(bounds, color);
    }

    void UILayer::OnWindowResize(int width, int height)
    {
        m_UIManager->SetScreenSize(width, height);

        Renderer::UpdateUIProjectionMatrix(width, height);
    }

    void UILayer::HandleInput()
    {
        auto mousePos = Input::GetMousePosition();
        bool leftClick = Input::IsMouseButtonPressed(0); 
        bool rightClick = Input::IsMouseButtonPressed(1); 

        // Pass input to UI manager
        m_UIManager->HandleMouseInput(mousePos.first, mousePos.second, leftClick, rightClick);

        // Handle debug toggle
        static bool lastF1State = false;
        bool currentF1State = Input::IsKeyPressed(VK_F1);

        if (currentF1State && !lastF1State) // Key pressed (not held)
        {
            m_ShowDebugInfo = !m_ShowDebugInfo;
            m_UIManager->SetDebugMode(m_ShowDebugInfo);

            std::string debugMsg = m_ShowDebugInfo ? "UI Debug mode enabled\n" : "UI Debug mode disabled\n";
            OutputDebugStringA(debugMsg.c_str());
        }

        lastF1State = currentF1State;
    }

    void UILayer::CreateExampleUI()
    {
        auto mainPanel = CreatePanel(UIRect(50, 50, 300, 400), UIColor(0.1f, 0.1f, 0.1f, 0.9f));

        // Create title text
        auto titleText = CreateText("DXEngine UI Demo", UIRect(60, 70, 280, 30));
        titleText->SetColor(UIColor(1.0f, 1.0f, 1.0f, 1.0f));
        titleText->SetFontSize(24.0f);
        titleText->SetAlignment(TextAlignment::Center);

        // Create some buttons
        auto startButton = CreateButton("Start Game", UIRect(70, 120, 260, 40));
        startButton->SetOnClick([this]() {
            OutputDebugStringA("Start Game button clicked!\n");
            });

        auto settingsButton = CreateButton("Settings", UIRect(70, 180, 260, 40));
        settingsButton->SetOnClick([this]() {
            OutputDebugStringA("Settings button clicked!\n");
            });

        auto exitButton = CreateButton("Exit", UIRect(70, 240, 260, 40));
        exitButton->SetNormalColor(UIColor(0.4f, 0.1f, 0.1f, 1.0f));
        exitButton->SetHoverColor(UIColor(0.6f, 0.2f, 0.2f, 1.0f));
        exitButton->SetPressedColor(UIColor(0.3f, 0.05f, 0.05f, 1.0f));
        exitButton->SetOnClick([this]() {
            OutputDebugStringA("Exit button clicked!\n");
            // You could post a window close event here
            });

        // Create status text
        auto statusText = CreateText("Press F1 to toggle debug mode", UIRect(70, 320, 260, 20));
        statusText->SetColor(UIColor(0.7f, 0.7f, 0.7f, 1.0f));
        statusText->SetFontSize(12.0f);

        // Create a side panel for additional info
        auto sidePanel = CreatePanel(UIRect(400, 50, 200, 200), UIColor(0.2f, 0.2f, 0.3f, 0.8f));

        auto infoText = CreateText("Engine Info", UIRect(410, 70, 180, 20));
        infoText->SetColor(UIColor(1.0f, 1.0f, 0.8f, 1.0f));
        infoText->SetFontSize(16.0f);

        auto versionText = CreateText("Version: 1.0.0", UIRect(410, 100, 180, 15));
        versionText->SetColor(UIColor(0.8f, 0.8f, 0.8f, 1.0f));
        versionText->SetFontSize(12.0f);

        auto fpsText = CreateText("FPS: 60", UIRect(410, 120, 180, 15));
        fpsText->SetColor(UIColor(0.8f, 0.8f, 0.8f, 1.0f));
        fpsText->SetFontSize(12.0f);

        OutputDebugStringA("Example UI created successfully\n");
    }
}