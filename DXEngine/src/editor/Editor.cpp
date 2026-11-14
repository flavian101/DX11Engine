#include "dxpch.h"
#include "Editor.h"
#include "ImGuizmoGizmo.h"
#include "SceneEditing.h"
#include "Application.h"
#include "camera/Camera.h"
#include "renderer/Renderer.h"

#include "imgui.h"
#include "imgui_internal.h"      // DockBuilder: the default layout

namespace DXEngine
{
    Editor::Editor()
    {
        m_Ctx.gizmo = std::make_unique<ImGuizmoGizmo>();
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

    void Editor::OnImGui()
    {
        if (!m_Enabled) return;
        ImGuiLayer& ui = Application::Get().GetImGuiLayer();
        if (!ui.IsActive()) return;                     // no UI backend (not D3D11): the scene simply stays in the window

        m_Ctx.flying = IsFlying();

        DrawMainMenu();
        DrawDockspace();

        m_Hierarchy.OnImGui(m_Ctx, &m_ShowHierarchy);
        m_Inspector.OnImGui(m_Ctx, &m_ShowInspector);
        m_Viewport.OnImGui(m_Ctx, &m_ShowViewport);
        RendererPanel::OnImGui(&m_ShowRenderer);

        // Over the scene picture the mouse belongs to the scene (picking, cursor capture); everywhere else to the panels.
        ui.SetBlockMouseEvents(!m_Ctx.viewportHovered);
    }

    void Editor::DrawMainMenu()
    {
        if (!ImGui::BeginMainMenuBar()) return;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit", "Esc"))
                Application::Get().GetWindow().QuitWindow();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create"))
        {
            DrawCreateMenu(m_Ctx, Entity());
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &m_ShowInspector);
            ImGui::MenuItem("Viewport",  nullptr, &m_ShowViewport);
            ImGui::MenuItem("Renderer",  nullptr, &m_ShowRenderer);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset layout"))
                m_ResetLayout = true;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
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

        // First run (no layout in editor_layout.ini) or "Reset layout": hierarchy left, inspector + renderer right, viewport in the middle.
        if (ImGui::DockBuilderGetNode(dockId) == nullptr || m_ResetLayout)
        {
            m_ResetLayout = false;
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockId, viewport->WorkSize);

            ImGuiID left = 0, rest = 0, right = 0, center = 0;
            left = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.20f, nullptr, &rest);
            right = ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.26f, nullptr, &center);

            ImGui::DockBuilderDockWindow("Hierarchy", left);
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderDockWindow("Renderer", right);
            ImGui::DockBuilderDockWindow("Inspector", right);      // docked last: the tab that is shown
            ImGui::DockBuilderFinish(dockId);
        }

        ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }
}
