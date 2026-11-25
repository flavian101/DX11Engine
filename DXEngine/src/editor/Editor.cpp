#include "dxpch.h"
#include "Editor.h"
#include "assets/ModelLoaderAdapter.h"
#include "EditorChrome.h"
#include "ImGuiLayer.h"
#include "EditorIcons.h"
#include "EditorTheme.h"
#include "ImGuizmoGizmo.h"
#include "SceneEditing.h"
#include "PlatformDialogs.h"
#include "assets/EngineAssetBridge.h"
#include "utils/AssetNames.h"
#include <filesystem>
#include "Application.h"
#include "Event/MouseEvent.h"
#include "camera/Camera.h"
#include "renderer/Renderer.h"

#include "imgui.h"
#include "imgui_internal.h"      // DockBuilder: the default layout

namespace DXEngine
{
    Editor::Editor()
    {
        m_Ctx.gizmo = std::make_unique<ImGuizmoGizmo>();
        m_Prefs = LoadEditorPrefs();
        // imports read the file with the engine's loader, on the device the editor holds when the first one runs
        m_Assets.Importer().SetLoadFunction(MakeModelLoaderFunction([this] { return m_Ctx.device; }));

        // Scene files: the real dialogs, the engine's asset bridge (names are relative to the working directory, where the editor
        // finds its assets), and the Assets panel's log for what saving and loading have to say.
        SceneFileController::Hooks hooks;
        hooks.pickOpenPath = [] { return PlatformDialogs::OpenSceneFile(); };
        hooks.pickSavePath = [](const std::string& suggested) { return PlatformDialogs::SaveSceneFile(suggested); };
        hooks.makeBridge = [this] { return MakeBridge(); };
        hooks.log = [this](const std::string& line, bool warning) { LogLine("scene: " + line, warning); };
        hooks.pathChanged = [this](const std::string& path) { RememberScene(path); };
        m_SceneFile = std::make_unique<SceneFileController>(std::move(hooks));
    }

    std::unique_ptr<IAssetBridge> Editor::MakeBridge()
    {
        auto device = m_Ctx.device ? m_Ctx.device : Renderer::PrimaryDevice();
        return std::make_unique<EngineAssetBridge>(device, m_Ctx.models, m_Assets.Importer(), m_Assets.MutableLog(), ProjectRoot());
    }

    std::string Editor::ProjectRoot() const
    {
        return std::filesystem::current_path().generic_string();
    }

    void Editor::RememberScene(const std::string& path)
    {
        const std::string stored = path.empty() ? std::string() : AssetNames::ToStoredPath(path, ProjectRoot());
        if (stored == m_Prefs.lastScene) return;
        m_Prefs.lastScene = stored;
        SaveEditorPrefs(m_Prefs);
    }

    bool Editor::OpenLastScene()
    {
        if (!m_Ctx.scene || m_Prefs.lastScene.empty()) return false;
        const std::string path = AssetNames::FromStoredPath(m_Prefs.lastScene, ProjectRoot());
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec))
        {
            LogLine("start-up: the last scene '" + m_Prefs.lastScene + "' is gone, starting with the default scene", true);
            return false;
        }
        return m_SceneFile->OpenFile(m_Ctx, path);          // (a file that cannot be read says so in the log and leaves the scene alone)
    }

    void Editor::LogLine(const std::string& line, bool warning)
    {
        if (warning) m_Assets.MutableLog().Warning(line);
        else         m_Assets.MutableLog().Info(line);
    }

    void Editor::Notify(const std::string& text)
    {
        m_Notice = text;
        m_NoticeSeconds = 8.0f;
    }

    Editor::~Editor()
    {
        if (m_Ctx.camera) m_Ctx.camera->SetInputGate(nullptr);        // the gate points at this editor
    }

    void Editor::SetCamera(std::shared_ptr<CameraController> camera)
    {
        if (m_Ctx.camera) m_Ctx.camera->SetInputGate(nullptr);
        m_Ctx.camera = std::move(camera);
        if (m_Ctx.camera)
        {
            // In the editor the camera listens only while it is flown (middle button held over the viewport): the keys
            // that move it are the ones a text field needs. In play mode it always listens. Asked live, per event.
            m_Ctx.camera->SetInputGate([this] { return !m_Enabled || IsFlying(); });
            m_Ctx.camera->SetMoveSpeed(m_Prefs.flySpeed);
        }
        ApplyCameraMode();
    }

    void Editor::SetGizmo(std::unique_ptr<IGizmo> gizmo)
    {
        m_Ctx.gizmo = gizmo ? std::move(gizmo) : std::make_unique<NullGizmo>();
    }

    // ---------------------------------------------------------------- play mode
    bool Editor::Play()
    {
        if (IsPlaying()) return false;
        if (!m_Ctx.scene) { Notify("Nothing to play: there is no scene"); return false; }
        m_Ctx.device = Renderer::Get().GetDevice();

        // What the editor holds by entity handle and wants back after the scene is restored: the stress-test parent, then every
        // expanded row (the selection is PlayMode's own).
        std::vector<Entity> keep;
        keep.push_back(m_StressTest);
        for (Entity e : m_Hierarchy.Model().ExpandedEntities(*m_Ctx.scene)) keep.push_back(e);

        const PlayMode::Outcome r = m_Play.Start(m_Ctx, m_Game, MakeBridge(), keep);
        for (const std::string& w : r.warnings) LogLine("play: " + w, true);
        if (!r.ok)
        {
            LogLine("play: " + r.error, true);
            std::string text = "Cannot play: " + (r.warnings.empty() ? r.error : r.warnings.front());
            if (r.warnings.size() > 1) text += "  (+" + std::to_string(r.warnings.size() - 1) + " more in the Assets log)";
            Notify(text);
            return false;
        }

        m_Notice.clear();
        m_NoticeSeconds = 0.0f;
        SetUiEnabled(false);
        return true;
    }

    void Editor::Stop()
    {
        if (!IsPlaying()) return;
        const PlayMode::Outcome r = m_Play.Stop(m_Ctx, m_Game, MakeBridge());
        for (const std::string& w : r.warnings) LogLine("play: " + w, true);

        // the scene has new entities (or, when it could not be restored, still the old ones): the handles the editor kept come back
        // through 'kept', in the order they were handed over
        if (!r.kept.empty())
        {
            m_StressTest = r.kept.front();
            for (size_t i = 1; i < r.kept.size(); ++i) m_Hierarchy.Model().SetExpanded(r.kept[i], true);
        }

        if (!r.ok)
        {
            LogLine("play: the scene could not be put back: " + r.error, true);
            Notify("The scene could not be put back (" + r.error + "): it keeps what the game made of it");
        }
        else if (!r.warnings.empty())
            Notify("Scene put back with " + std::to_string(r.warnings.size()) + (r.warnings.size() == 1 ? " warning" : " warnings") + " (see the Assets log)");

        SetUiEnabled(true);
    }

    void Editor::Update(float deltaSeconds)
    {
        if (!IsPlaying() || !m_Game || !m_Ctx.scene) return;
        if (m_Game->Update(*m_Ctx.scene, deltaSeconds)) return;

        // a system threw: the game has stopped itself. End the session, so the scene is back and the error is not lost.
        const std::string why = m_Game->LastError();
        LogLine("play: the game stopped: " + why, true);
        Stop();
        Notify("Play stopped: " + why);
    }

    void Editor::SetUiEnabled(bool enabled)
    {
        if (m_Enabled == enabled) return;
        m_Enabled = enabled;
        m_Ctx.viewportHovered = m_Ctx.viewportFocused = false;
        if (!m_Enabled)
        {
            Renderer::Get().SetSceneTargetSize(0, 0);                       // back into the window
            ImGuiLayer::Get().SetBlockMouseEvents(true);
            m_PlayHint = 4.0f;
        }
        ApplyCameraMode();
    }

    // Editor on: the camera's aspect is the panel's. Off: it is the window's again.
    void Editor::ApplyCameraMode()
    {
        if (!m_Ctx.camera) return;
        m_Ctx.camera->SetFollowWindowAspect(!m_Enabled);
        if (!m_Enabled)
        {
            Window& w = Application::Get().GetWindow();
            if (w.GetWidth() > 0 && w.GetHeight() > 0)
                m_Ctx.camera->GetCamera()->SetAspectRatio(static_cast<float>(w.GetWidth()) / static_cast<float>(w.GetHeight()));
        }
    }

    bool Editor::IsFlying() const
    {
        return Application::HasInstance() && Application::Get().GetWindow().IsCursorCaptured();
    }

    bool Editor::AcceptsGameInput() const
    {
        if (!m_Enabled) return true;
        return m_Ctx.viewportFocused && !ImGuiLayer::Get().WantsTextInput();
    }

    bool Editor::OnEvent(Event& e)
    {
        if (!m_Enabled || !m_Ctx.camera || !IsFlying()) return false;
        if (e.GetEventType() != EventType::MouseScrolled) return false;

        const auto& scroll = static_cast<MouseScrolledEvent&>(e);
        const float speed = ViewportMath::AdjustFlySpeed(m_Ctx.camera->GetMoveSpeed(), scroll.GetYOffset());
        m_Ctx.camera->SetMoveSpeed(speed);
        m_Prefs.flySpeed = speed;
        SaveEditorPrefs(m_Prefs);
        e.Handled = true;
        return true;
    }

    void Editor::SetUiScale(float scale)
    {
        m_Prefs.uiScale = scale;
        EditorTheme::Apply(scale);
        SaveEditorPrefs(m_Prefs);
    }

    void Editor::ToggleStressTest()
    {
        if (!m_Ctx.scene) return;
        if (m_StressTest.IsValid())
        {
            m_Ctx.scene->DestroyEntity(m_StressTest);       // the lights go with their parent
            m_StressTest = Entity();
        }
        else
            m_StressTest = SceneEditing::AddStressLights(*m_Ctx.scene, 1000);
    }

    void Editor::OnImGui()
    {
        ImGuiLayer& ui = ImGuiLayer::Get();
        if (!ui.IsActive()) return;                     // no UI backend (not D3D11): the scene simply stays in the window

        if (!m_ThemeApplied)
        {
            m_ThemeApplied = true;
            EditorTheme::Apply(m_Prefs.uiScale);
        }

        // V-Sync is a renderer setting the panel changes directly: the saved choice is applied once, then the renderer
        // leads and a change is saved (also while playing, where the panel is hidden but a key may change it)
        if (!m_VSyncApplied)
        {
            m_VSyncApplied = true;
            Renderer::Get().SetVSync(m_Prefs.vsync);
        }
        else if (Renderer::Get().IsVSyncEnabled() != m_Prefs.vsync)
        {
            m_Prefs.vsync = Renderer::Get().IsVSyncEnabled();
            SaveEditorPrefs(m_Prefs);
        }

        if (!m_Enabled)
        {
            DrawPlayHint();
            return;
        }

        m_Ctx.flying = IsFlying();
        m_Ctx.device = Renderer::Get().GetDevice();

        // The order matters: the bars first (each one shrinks the area left), then the dock space inside what is left.
        DrawMainMenu();
        if (EditorChrome::DrawToolbar(m_Ctx).playPressed && Play())
        {
            DrawPlayHint();                              // the scene fills the window from here: no panels this frame
            return;
        }

        if (m_NoticeSeconds > 0.0f) m_NoticeSeconds -= ImGui::GetIO().DeltaTime;
        const char* hint = m_NoticeSeconds > 0.0f ? m_Notice.c_str()
                         : m_Ctx.flying          ? "Release the middle button to stop flying"
                         : m_Ctx.viewportHovered ? "Middle button + W A S D  fly      Left click  select      F  frame      Q W E R  tools"
                                                 : "";
        EditorChrome::DrawStatusBar(m_Ctx, hint);
        DrawDockspace();

        // file shortcuts work anywhere in the editor except while a text field is being edited
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && !ui.WantsTextInput())
            {
                if (ImGui::IsKeyPressed(ImGuiKey_S, false)) { if (io.KeyShift) m_SceneFile->SaveAs(m_Ctx); else m_SceneFile->Save(m_Ctx); }
                else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) m_SceneFile->Open(m_Ctx);
                else if (ImGui::IsKeyPressed(ImGuiKey_N, false)) m_SceneFile->New(m_Ctx);
            }
        }
        m_SceneFile->OnImGui(m_Ctx);                       // the "unsaved changes" question, while one is pending

        m_Hierarchy.OnImGui(m_Ctx, &m_ShowHierarchy);
        m_Inspector.OnImGui(m_Ctx, &m_ShowInspector);
        m_Viewport.OnImGui(m_Ctx, &m_ShowViewport);
        m_SecondView.OnImGui(m_Ctx, &m_ShowSecondView);
        RendererPanel::OnImGui(&m_ShowRenderer);
        m_Assets.Update(m_Ctx);                                    // carries out a held import, also while the window is hidden
        m_Assets.OnImGui(m_Ctx, &m_ShowAssets);
        DrawShortcutsWindow();

        // Over the scene picture the mouse belongs to the scene (picking, cursor capture); everywhere else to the panels.
        ui.SetBlockMouseEvents(!m_Ctx.viewportHovered);
    }

    void Editor::DrawPlayHint()
    {
        if (m_PlayHint <= 0.0f) return;
        m_PlayHint -= ImGui::GetIO().DeltaTime;

        const char* text = "Playing   -   F1 stops and puts the scene back";
        const ImVec2 size = ImGui::CalcTextSize(text);
        const float pad = 10.0f * EditorTheme::Scale();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const ImVec2 min((display.x - size.x) * 0.5f - pad, 16.0f);
        const ImVec2 max(min.x + size.x + pad * 2.0f, min.y + size.y + pad);
        const float fade = std::min(1.0f, m_PlayHint);                    // fades out over the last second
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        dl->AddRectFilled(min, max, IM_COL32(12, 14, 20, static_cast<int>(210 * fade)), 6.0f);
        dl->AddText(ImVec2(min.x + pad, min.y + pad * 0.5f), IM_COL32(235, 237, 242, static_cast<int>(255 * fade)), text);
    }

    void Editor::DrawMainMenu()
    {
        if (!ImGui::BeginMainMenuBar()) return;
        const bool hasSelection = m_Ctx.selection.Has();
        Scene* scene = m_Ctx.scene;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene", "Ctrl+N", false, scene != nullptr))
                m_SceneFile->New(m_Ctx);
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O", false, scene != nullptr))
                m_SceneFile->Open(m_Ctx);
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S", false, scene != nullptr))
                m_SceneFile->Save(m_Ctx);
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, scene != nullptr))
                m_SceneFile->SaveAs(m_Ctx);
            ImGui::Separator();
            if (ImGui::MenuItem("Import Model...", nullptr, false, !m_Assets.IsBusy()))
                m_Assets.PickAndImport();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Esc"))
                Application::Get().GetWindow().QuitWindow();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Rename", "F2", false, hasSelection))
                m_Hierarchy.BeginRename(m_Ctx.selection.Get());
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasSelection && scene && SceneEditing::CanDuplicate(*scene, m_Ctx.selection.Get())))
            {
                Entity copy = SceneEditing::Duplicate(*scene, m_Ctx.selection.Get());
                if (copy.IsValid()) { m_Ctx.selection.Set(copy); m_Ctx.revealSelection = true; }
            }
            if (ImGui::MenuItem("Delete", "Del", false, hasSelection && scene))
                scene->DestroyEntity(m_Ctx.selection.Get());
            ImGui::Separator();
            if (ImGui::MenuItem("Frame selected", "F", false, hasSelection))
                m_Viewport.FrameSelection(m_Ctx);
            if (ImGui::MenuItem("Select nothing", nullptr, false, hasSelection))
                m_Ctx.selection.Clear();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create"))
        {
            DrawCreateMenu(m_Ctx, Entity());
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &m_ShowInspector);
            ImGui::MenuItem("Viewport",  nullptr, &m_ShowViewport);
            ImGui::MenuItem("View 2",    nullptr, &m_ShowSecondView);
            ImGui::MenuItem("Renderer",  nullptr, &m_ShowRenderer);
            ImGui::MenuItem("Assets",    nullptr, &m_ShowAssets);
            ImGui::Separator();
            if (ImGui::BeginMenu("UI scale"))
            {
                for (float s : EditorTheme::kScales)
                {
                    char label[16];
                    snprintf(label, sizeof(label), "%.0f%%", s * 100.0f);
                    if (ImGui::MenuItem(label, nullptr, std::fabs(EditorTheme::Scale() - s) < 0.01f))
                        SetUiScale(s);
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Reset layout"))
                m_ResetLayout = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Play", "F1"))
                Play();
            ImGui::Separator();
            if (ImGui::MenuItem("Stress test: 1000 point lights", nullptr, m_StressTest.IsValid(), scene != nullptr))
                ToggleStressTest();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Keyboard shortcuts", nullptr, &m_ShowShortcuts);
            ImGui::EndMenu();
        }
        // the scene's file, at the right end of the bar
        {
            const std::string label = m_SceneFile->Path().empty() ? std::string("unsaved scene") : std::filesystem::path(m_SceneFile->Path()).filename().string();
            const float width = ImGui::CalcTextSize(label.c_str()).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - width - 16.0f * EditorTheme::Scale());
            ImGui::TextDisabled("%s", label.c_str());
        }
        ImGui::EndMainMenuBar();
    }

    void Editor::DrawShortcutsWindow()
    {
        if (!m_ShowShortcuts) return;
        ImGui::SetNextWindowSize(ImVec2(440.0f * EditorTheme::Scale(), 0.0f), ImGuiCond_Appearing);
        if (ImGui::Begin("Keyboard shortcuts", &m_ShowShortcuts, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
        {
            static const struct { const char* keys; const char* what; } kRows[] = {
                { "Middle button + W A S D", "Fly the camera;  Space: up,  Left Ctrl: down" },
                { "Mouse wheel while flying", "Change the fly speed" },
                { "Left click", "Select the object under the cursor" },
                { "Q  W  E  R", "Select / move / rotate / scale tool" },
                { "F", "Frame the selection" },
                { "F2", "Rename (hierarchy)" },
                { "Ctrl + D", "Duplicate" },
                { "Delete", "Delete" },
                { "F1", "Play / Stop (Stop puts the scene back as it was)" },
                { "Esc", "Quit (not while typing in a field)" },
            };
            if (ImGui::BeginTable("##shortcuts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX))
            {
                ImGui::TableSetupColumn("keys", ImGuiTableColumnFlags_WidthFixed, 190.0f * EditorTheme::Scale());
                ImGui::TableSetupColumn("what", ImGuiTableColumnFlags_WidthStretch);
                for (const auto& r : kRows)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(EditorTheme::Colors().accent, "%s", r.keys);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(r.what);
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void Editor::DrawDockspace()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        const ImGuiWindowFlags host = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                      ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##EditorDockHost", nullptr, host);
        ImGui::PopStyleVar(3);

        const ImGuiID dockId = ImGui::GetID("EditorDockSpace");

        // First run (no layout in editor_layout.ini) or "Reset layout": hierarchy and renderer on the left, the inspector on
        // the right, the viewport in the middle.
        if (ImGui::DockBuilderGetNode(dockId) == nullptr || m_ResetLayout)
        {
            m_ResetLayout = false;
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockId, viewport->WorkSize);

            ImGuiID left = 0, rest = 0, right = 0, center = 0, leftTop = 0, leftBottom = 0;
            left = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.20f, nullptr, &rest);
            right = ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.26f, nullptr, &center);
            leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.42f, nullptr, &leftTop);

            ImGui::DockBuilderDockWindow("Hierarchy", leftTop);
            ImGui::DockBuilderDockWindow("Renderer", leftBottom);
            ImGui::DockBuilderDockWindow("Assets", leftBottom);
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderDockWindow("Inspector", right);
            ImGui::DockBuilderFinish(dockId);
        }

        ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }
}
