#pragma once
#include "EditorContext.h"
#include "EditorPrefs.h"
#include "PlayMode.h"
#include "AssetsPanel.h"
#include "HierarchyPanel.h"
#include "SceneFileController.h"
#include "InspectorPanel.h"
#include "RendererPanel.h"
#include "SecondViewPanel.h"
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
    // While the editor is showing, the scene is drawn into a texture shown in the Viewport panel (Renderer::SetSceneTargetSize)
    // and the camera's aspect ratio follows that panel. Play puts the scene into the window and starts the game (the
    // GameRuntime the host gave: SetGame); Stop ends the game and puts the scene back exactly as it was when Play was pressed
    // (PlayMode: a snapshot in memory). The host calls Update(dt) once per frame, before it runs its own systems, so the game
    // runs on the live scene while playing.
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

        // The starting scene: the file last opened or saved (editor_prefs.ini remembers it). True when it was opened; false when there is
        // none, it is gone or it could not be read (the Assets log says which): the caller then builds its own scene. Call it once,
        // after SetScene, before the first frame.
        bool OpenLastScene();

        // The game that runs between Play and Stop. The host owns it and registers its systems; null = playing without gameplay
        // (animations run, the camera flies).
        void SetGame(GameRuntime* game) { m_Game = game; }

        // Play: the scene is saved in memory, fills the window and the game starts. False (nothing changes, the status bar says
        // why) when the scene cannot be restored afterwards (an attachment without a name a file could use) or the game cannot
        // start. Safe from a UI frame: it only reads the scene (the game's OnStart may change it).
        bool Play();
        // Stop: the game stops, the scene is put back as it was and the editor shows again; the selection, the expanded rows and the
        // editor camera are where they were. It replaces the scene's contents (every entity handle dies): call it from the host's
        // OnUpdate (F1), not in the middle of a UI frame.
        void Stop();
        bool IsPlaying() const { return !m_Enabled; }

        // Once per frame from the host's OnUpdate, before its own systems (gameplay first, then animation and transforms, then
        // drawing): runs the game while playing. A game that fails ends the play session (Stop) and the status bar says what happened.
        void Update(float deltaSeconds);

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
        void SetUiEnabled(bool enabled);                // the editor's panels or the full-window scene (Play and Stop use it)
        std::unique_ptr<IAssetBridge> MakeBridge();      // names and finds assets, as scene files do
        std::string ProjectRoot() const;                  // what stored asset and scene paths are relative to (the working directory: Main.h entered the root)
        void RememberScene(const std::string& path);      // the scene file changed: it is the starting scene next time
        void LogLine(const std::string& line, bool warning);
        void Notify(const std::string& text);            // a message in the status bar for a few seconds
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
        SecondViewPanel m_SecondView;
        AssetsPanel    m_Assets;
        std::unique_ptr<SceneFileController> m_SceneFile;   // New / Open / Save / Save As (built in the constructor: its hooks need the panels)
        PlayMode       m_Play;
        GameRuntime*   m_Game = nullptr;            // the host's, may be null
        EditorPrefs    m_Prefs;
        Entity         m_StressTest;                // the parent of the stress lights while they exist

        bool  m_Enabled = true;
        bool  m_ThemeApplied = false;
        bool  m_VSyncApplied = false;     // the saved V-Sync choice reaches the renderer once; after that the renderer leads
        bool  m_ShowHierarchy = true;
        bool  m_ShowInspector = true;
        bool  m_ShowViewport  = true;
        bool  m_ShowSecondView = false;      // View > View 2: the scene through a second camera
        bool  m_ShowRenderer  = true;
        bool  m_ShowAssets    = true;
        bool  m_ShowShortcuts = false;
        bool  m_ResetLayout   = false;
        float m_PlayHint = 0.0f;                    // seconds left of the "F1 to stop" hint after entering play mode
        std::string m_Notice;                       // a message for the status bar (a refused Play, a restore with warnings) ...
        float m_NoticeSeconds = 0.0f;               // ... and how long it still shows
    };
}
