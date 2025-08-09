#pragma once
#include "Core/Layer.h"
#include "utils/UI/UIManager.h"
#include <memory>

namespace DXEngine
{
    class UILayer : public Layer
    {
    public:
        UILayer(const std::string& name = "UILayer");
        virtual ~UILayer();

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnUpdate(FrameTime dt) override;
        virtual void OnUIRender() override;
        virtual void OnEvent(Event& event) override;

        // UI Management
        UIManager* GetUIManager() { return m_UIManager.get(); }

        // Quick access factory methods
        std::shared_ptr<UIButton> CreateButton(const std::string& text, const UIRect& bounds);
        std::shared_ptr<UIText> CreateText(const std::string& text, const UIRect& bounds);
        std::shared_ptr<UIPanel> CreatePanel(const UIRect& bounds, const UIColor& color = UIColor(0.2f, 0.2f, 0.2f, 0.8f));

        // Screen management
        void OnWindowResize(int width, int height);

        void HandleInput();

        void CreateExampleUI();

    private:
        std::unique_ptr<UIManager> m_UIManager;
        bool m_ShowDebugInfo = false;
    };
}