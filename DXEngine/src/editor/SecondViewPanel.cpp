#include "dxpch.h"
#include "SecondViewPanel.h"
#include "camera/Camera.h"
#include "camera/CameraController.h"
#include "renderer/Renderer.h"
#include "RHI/GraphicsDevice.h"
#include "scene/Scene.h"
#include "scene/TransformSystem.h"
#include "imgui.h"

namespace DXEngine
{
    SecondViewPanel::SecondViewPanel() = default;
    SecondViewPanel::~SecondViewPanel() = default;

    void SecondViewPanel::Pin(EditorContext& ctx)
    {
        if (!m_Camera) return;
        if (ctx.camera)
        {
            const auto source = ctx.camera->GetCamera();
            m_Camera->SetPosition(source->GetPositionFloat3());
            m_Camera->SetRotation(source->GetRotationFloat3());
        }
        m_Camera->GetViewMatrix4X4();              // refreshes the cached view matrix (the renderer reads the cache)
    }

    void SecondViewPanel::OnImGui(EditorContext& ctx, bool* open)
    {
        if (open && !*open)
        {
            m_View.reset();                        // closed: give the texture back
            return;
        }

        if (!ImGui::Begin("View 2", open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::End();                          // a collapsed / hidden tab: nothing to draw, nothing to size
            return;
        }

        if (!ctx.scene || !Renderer::HasInstance())
        {
            ImGui::TextDisabled("No scene to show");
            ImGui::End();
            return;
        }

        Renderer& renderer = Renderer::Get();
        if (!m_View)
            m_View = renderer.CreateView("View 2");
        if (!m_Camera)
        {
            m_Camera = std::make_shared<Camera>();
            Pin(ctx);                              // starts where the editor camera is
        }

        if (ImGui::Button("Pin to editor camera"))
            Pin(ctx);
        ImGui::SameLine();
        ImGui::TextDisabled("%d x %d", m_Size.width, m_Size.height);

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ViewportMath::Size target = ViewportMath::ChooseTargetSize(avail.x, avail.y, m_Size);
        if (target.width != m_Size.width || target.height != m_Size.height)
        {
            m_Size = target;
            m_Camera->SetAspectRatio(ViewportMath::AspectRatio(m_Size.width, m_Size.height));
        }

        RHI::ITexture* texture = nullptr;
        if (m_View)                                // null when there is no renderer to make views
        {
            m_View->SetSize(m_Size.width, m_Size.height);
            const float* clear = renderer.GetMainView().GetClearColor();
            m_View->SetClearColor(clear[0], clear[1], clear[2], clear[3]);

            // A gizmo drag earlier in this very frame may have moved things: the renderer only reads world matrices.
            TransformSystem::Update(*ctx.scene);
            renderer.Render(*m_View, ctx.scene, m_Camera);
            texture = m_View->GetTexture();
        }

        if (texture)
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texture->GetNativeHandle())), avail);
        else
            ImGui::Dummy(avail);                   // before the first draw: the texture does not exist yet
        ImGui::End();
    }
}
