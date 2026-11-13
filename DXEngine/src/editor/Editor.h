#pragma once
#include "EditorContext.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "RendererPanel.h"
#include "ViewportPanel.h"

namespace DXEngine
{
    // ==================================================================
    // Editor: the dockspace, the main menu and the panels (Hierarchy, Inspector, Viewport, Renderer), driven by one call.
    //
    //     Editor editor;                                   // a member of the game's layer
    //     editor.SetScene(&scene);                         // the host owns both; the editor edits them
    //     editor.SetCamera(cameraController);
    //     ...
    //     void MyLayer::OnUIRender() { editor.OnImGui(); }
    //     void MyLayer::OnUpdate(dt) {                     // keys that would also type into a text field:
    //         cameraController->SetInputEnabled(editor.IsFlying());
    //         if (editor.AcceptsGameInput()) DetectInput(dt);
    //     }
    //
    // While the editor is enabled the scene is drawn into a texture shown in the Viewport panel (Renderer::SetSceneTargetSize)
    // and the camera's aspect ratio follows that panel. Disabling it ("play mode") puts the scene back into the window.
    // ==================================================================
    class Editor
    {
    public:
        void SetScene(Scene* scene) { m_Ctx.scene = scene; }
        void SetCamera(std::shared_ptr<CameraController> camera);
        void SetGizmo(std::unique_ptr<IGizmo> gizmo);          // default: NullGizmo (see GizmoTypes.h)

        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_Enabled; }

        // Build all the windows for this frame. Call from a Layer's OnUIRender.
        void OnImGui();

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
        void DrawMainMenu();
        void DrawDockspace();

        EditorContext m_Ctx;
        HierarchyPanel m_Hierarchy;
        InspectorPanel m_Inspector;
        ViewportPanel  m_Viewport;

        bool m_Enabled = true;
        bool m_ShowHierarchy = true;
        bool m_ShowInspector = true;
        bool m_ShowViewport  = true;
        bool m_ShowRenderer  = true;
        bool m_ResetLayout   = false;
    };
}
