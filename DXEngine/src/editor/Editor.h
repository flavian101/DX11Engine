#pragma once
#include "EditorContext.h"
#include "EditorPrefs.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "RendererPanel.h"
#include "ViewportPanel.h"
#include "Event/Event.h"

namespace DXEngine
{
    // ==================================================================
    // Editor: the main menu, the toolbar, the status bar, the dock space and the panels (Hierarchy, Inspector, Viewport,
    // Renderer), driven by one call.
    //
    //     Editor editor;                                   // a member of the game's layer
    //     editor.SetScene(&scene);                         // the host owns both; the editor edits them
    //     editor.SetCamera(cameraController);              // also decides when the camera listens (while it is flown)
    //     ...
    //     void MyLayer::OnUIRender() { editor.OnImGui(); }
    //     void MyLayer::OnEvent(Event& e) { if (!editor.OnEvent(e)) camera->OnEvent(e); }
    //     void MyLayer::OnUpdate(dt) {
    //         if (editor.AcceptsGameInput()) PolledShortcuts(dt);   // keys that would also type into a text field
    //         ...
    //     }
    //
    // While the editor is enabled the scene is drawn into a texture shown in the Viewport panel (Renderer::SetSceneTargetSize)
    // and the camera's aspect ratio follows that panel. Disabling it ("play mode") puts the scene back into the window.
    // ==================================================================
    class Editor
    {
    public:
        Editor();                                   // installs the ImGuizmo gizmo (SetGizmo replaces it)
        ~Editor();
        Editor(const Editor&) = delete;
        Editor& operator=(const Editor&) = delete;

        void SetScene(Scene* scene) { m_Ctx.scene = scene; }
        void SetCamera(std::shared_ptr<CameraController> camera);
        void SetGizmo(std::unique_ptr<IGizmo> gizmo);          // default: ImGuizmo (see GizmoTypes.h); null = no gizmo

        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_Enabled; }

        // Build all the windows for this frame. Call from a Layer's OnUIRender.
        void OnImGui();

        // The mouse wheel changes the fly speed while the camera is being flown. Returns true when it took the event.
        bool OnEvent(Event& e);

        // The camera owns the mouse (middle button held, cursor captured): the moment to let it move. Asked of the window
        // itself, so it is right even in the frame the button went down.
        bool IsFlying() const;
        // The cursor is over the scene picture / the viewport panel has focus.
        bool IsViewportHovered() const { return m_Ctx.viewportHovered; }
        bool IsViewportFocused() const { return m_Ctx.viewportFocused; }
        // Keyboard shortcuts polled by the game should run only when this is true: the editor is off (play mode), or the
        // viewport has focus and no text field is being edited.
        bool AcceptsGameInput() const;

        EditorSelection& Selection() { return m_Ctx.selection; }
        EditorContext&   Context()   { return m_Ctx; }

    private:
        void ApplyCameraMode();
        void SetUiScale(float scale);
        void DrawMainMenu();
        void DrawDockspace();
        void DrawShortcutsWindow();
        void DrawPlayHint();
        void ToggleStressTest();

        EditorContext m_Ctx;
        HierarchyPanel m_Hierarchy;
        InspectorPanel m_Inspector;
        ViewportPanel  m_Viewport;
        EditorPrefs    m_Prefs;
        Entity         m_StressTest;                // the parent of the stress lights while they exist

        bool  m_Enabled = true;
        bool  m_ThemeApplied = false;
        bool  m_ShowHierarchy = true;
        bool  m_ShowInspector = true;
        bool  m_ShowViewport  = true;
        bool  m_ShowRenderer  = true;
        bool  m_ShowShortcuts = false;
        bool  m_ResetLayout   = false;
        float m_PlayHint = 0.0f;                    // seconds left of the "F1 to edit" hint after entering play mode
    };
}
