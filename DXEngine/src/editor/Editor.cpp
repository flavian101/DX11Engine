#include "dxpch.h"
#include "Editor.h"
#include "EditorChrome.h"
#include "EditorIcons.h"
#include "EditorTheme.h"
#include "ImGuizmoGizmo.h"
#include "SceneEditing.h"
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

    void Editor::SetEnabled(bool enabled)
    {
        if (m_Enabled == enabled) return;
        m_Enabled = enabled;
        m_Ctx.viewportHovered = m_Ctx.viewportFocused = false;
        if (!m_Enabled)
        {
            Renderer::SetSceneTargetSize(0, 0);                       // back into the window
            Application::Get().GetImGuiLayer().SetBlockMouseEvents(true);
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
        return m_Ctx.viewportFocused && !Application::Get().GetImGuiLayer().WantsTextInput();
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
        ImGuiLayer& ui = Application::Get().GetImGuiLayer();
        if (!ui.IsActive()) return;                     // no UI backend (not D3D11): the scene simply stays in the window

        if (!m_ThemeApplied)
        {
            m_ThemeApplied = true;
            EditorTheme::Apply(m_Prefs.uiScale);
        }
        if (!m_Enabled)
        {
            DrawPlayHint();
            return;
        }

        m_Ctx.flying = IsFlying();
        m_Ctx.device = Renderer::GetDevice();

        // The order matters: the bars first (each one shrinks the area left), then the dock space inside what is left.
        DrawMainMenu();
        if (EditorChrome::DrawToolbar(m_Ctx).togglePlay)
            SetEnabled(false);

        const char* hint = m_Ctx.flying          ? "Release the middle button to stop flying"
                         : m_Ctx.viewportHovered ? "Middle button + W A S D  fly      Left click  select      F  frame      Q W E R  tools"
                                                 : "";
        EditorChrome::DrawStatusBar(m_Ctx, hint);
        DrawDockspace();

        m_Hierarchy.OnImGui(m_Ctx, &m_ShowHierarchy);
        m_Inspector.OnImGui(m_Ctx, &m_ShowInspector);
        m_Viewport.OnImGui(m_Ctx, &m_ShowViewport);
        RendererPanel::OnImGui(&m_ShowRenderer);
        DrawShortcutsWindow();

        // Over the scene picture the mouse belongs to the scene (picking, cursor capture); everywhere else to the panels.
        ui.SetBlockMouseEvents(!m_Ctx.viewportHovered);
    }

    void Editor::DrawPlayHint()
    {
        if (m_PlayHint <= 0.0f) return;
        m_PlayHint -= ImGui::GetIO().DeltaTime;

        const char* text = "Playing   -   F1 returns to the editor";
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
            ImGui::MenuItem("Renderer",  nullptr, &m_ShowRenderer);
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
                SetEnabled(false);
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
                { "F1", "Switch between editor and play" },
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
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderDockWindow("Inspector", right);
            ImGui::DockBuilderFinish(dockId);
        }

        ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }
}
