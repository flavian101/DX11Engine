#pragma once
#include "Core/Layer.h"
#include <string>

namespace DXEngine
{
    // ==================================================================
    // ImGuiLayer: Dear ImGui (DOCKING branch) wired into the engine. Application owns one, as the top overlay.
    //
    // Frame, driven by Application::Render:
    //     layers OnUpdate   (the scene is drawn: BeginScene ... EndScene)
    //     ImGuiLayer::Begin()
    //     layers OnUIRender (panels build their windows)
    //     ImGuiLayer::End()  -> draws into the BACK BUFFER, on top of whatever the renderer put there
    //     Renderer::Present()
    //
    // Input: the layer sees raw window messages through Window::SetNativeMessageHook (that is what feeds ImGui),
    // and as a Layer it can swallow engine events the UI wants (a click on a panel must not also pick an entity).
    // Anything that is not UI, the editor's viewport for instance, switches the mouse part off with SetBlockMouseEvents(false).
    //
    // Only the Win32 + DirectX 11 backends exist. With another graphics API the layer stays inactive (IsActive() is false)
    // and every other method is a no-op, so callers do not have to check.
    // ==================================================================
    class ImGuiLayer : public Layer
    {
    public:
        ImGuiLayer();
        ~ImGuiLayer() override;

        void OnAttach() override;      // creates the context and the backends
        void OnDetach() override;      // idempotent. Application calls it BEFORE the renderer shuts down.
        void OnEvent(Event& e) override;

        void Begin();                  // starts the UI frame
        void End();                    // renders it to the back buffer

        bool IsActive() const { return m_Active; }

        // While true, mouse events the UI wants (the cursor is over a panel) are marked handled so nothing underneath reacts to
        // them. The editor turns it off while the cursor is over its viewport, where mouse events belong to the scene.
        // Keyboard events the UI wants (a text field is active) are always swallowed. Default: true.
        void SetBlockMouseEvents(bool block) { m_BlockMouseEvents = block; }

        // Cheap questions for code that polls input instead of listening to events.
        bool WantsMouse() const;       // mouse blocking is on AND the UI wants the mouse
        bool WantsKeyboard() const;    // the UI wants the keyboard (a text field, or keyboard navigation)
        bool WantsTextInput() const;   // a text field is being edited

        // The UI ignores the mouse completely (and does not touch the cursor). The editor uses it while the camera
        // owns the mouse (cursor captured): the pointer is re-centred every move and must not click or hover panels.
        void SetMouseEnabled(bool enabled);

    private:
        void ApplyStyle();

        bool m_Active = false;
        bool m_BlockMouseEvents = true;
        // ImGui keeps the POINTER to the ini file name, so it needs storage that outlives the context.
        std::string m_IniFile = "editor_layout.ini";
    };
}
