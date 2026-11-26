#include "dxpch.h"
#include "ViewportPanel.h"
#include "EntityBounds.h"
#include "SceneEditing.h"
#include "camera/Camera.h"
#include "renderer/Renderer.h"
#include "RHI/GraphicsDevice.h"
#include "scene/ScenePicking.h"

#include "imgui.h"

namespace DXEngine
{
    namespace
    {
        constexpr ImU32 kOutline = IM_COL32(255, 166, 26, 255);          // the primary entity
        constexpr ImU32 kOutlineOthers = IM_COL32(255, 200, 110, 190);   // the rest of a multi-selection

    }

    void ViewportPanel::OnImGui(EditorContext& ctx, bool* open)
    {
        ctx.viewportHovered = ctx.viewportFocused = false;
        if (open && !*open)
        {
            // hidden panel: the scene is still drawn (into a texture nobody shows) at the last size, never into the window
            Renderer::Get().SetSceneTargetSize(std::max(m_Size.width, 1), std::max(m_Size.height, 1));
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        const bool visible = ImGui::Begin("Viewport", open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        // size: whole pixels, and the previous size while the panel is collapsed / a hidden tab
        const ImVec2 avail = visible ? ImGui::GetContentRegionAvail() : ImVec2(0, 0);
        const ViewportMath::Size target = ViewportMath::ChooseTargetSize(avail.x, avail.y, m_Size);
        if (target.width != m_Size.width || target.height != m_Size.height)
        {
            m_Size = target;
            if (ctx.camera)       // the camera follows the PANEL, not the window (CameraController::SetFollowWindowAspect(false))
                ctx.camera->GetCamera()->SetAspectRatio(ViewportMath::AspectRatio(m_Size.width, m_Size.height));
        }
        Renderer::Get().SetSceneTargetSize(m_Size.width, m_Size.height);

        if (!visible)
        {
            ImGui::End();
            return;
        }

        // ---- the image ----
        RHI::ITexture* texture = Renderer::Get().GetSceneTexture();
        ImGui::SetNextItemAllowOverlap();             // the toolbar sits on top of it
        if (texture)
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texture->GetNativeHandle())), avail);
        else
            ImGui::Dummy(avail);                      // the first frame: the texture does not exist yet
        const bool overImage = ImGui::IsItemHovered();
        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const ImVec2 imageSize = ImGui::GetItemRectSize();
        m_ImageRect = { imageMin.x, imageMin.y, imageSize.x, imageSize.y };

        // ---- gizmo, outline, toolbar ----
        bool gizmoBusy = false;
        Entity selected = ctx.selection.Get();
        if (selected.IsValid() && ctx.camera && ctx.scene)
        {
            DrawSelectionOutline(ctx, imageMin.x, imageMin.y, imageSize.x, imageSize.y);

            if (ctx.gizmo->IsAvailable() && ctx.gizmoSettings.operation != GizmoOperation::None && selected.Has<TransformComponent>())
            {
                GizmoView view;
                view.view = ctx.camera->GetCamera()->GetViewMatrix4X4();
                view.projection = ctx.camera->GetCamera()->GetProjection4X4();
                view.x = imageMin.x; view.y = imageMin.y; view.width = imageSize.x; view.height = imageSize.y;

                const DirectX::XMFLOAT4X4 before = selected.Get<TransformComponent>().world;
                DirectX::XMFLOAT4X4 world = before;
                if (ctx.gizmo->Manipulate(view, ctx.gizmoSettings, world))
                {
                    // the gizmo sits on the primary; everything selected moves with it, as one rigid group
                    const std::vector<Entity> group = ctx.selection.All();
                    if (group.size() < 2 || !SceneEditing::ApplyWorldDelta(*ctx.scene, group, before, world))
                        SceneEditing::ApplyWorldMatrix(*ctx.scene, selected, world);
                    ctx.MarkEdited();                       // (the step is recorded when the drag ends)
                }
                gizmoBusy = ctx.gizmo->IsUsing() || ctx.gizmo->IsOver();
            }
        }
        DrawFlyHud(ctx, imageMin.x, imageMin.y, imageSize.x);

        // hovered = over the picture and not over a toolbar widget (those have an id, the picture has none)
        ctx.viewportHovered = overImage && !ImGui::IsAnyItemHovered();
        ctx.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        // ---- pick, or a box ----
        // A press in the picture starts it; the release decides: the mouse hardly moved = a click (pick), it moved = a box.
        if (ctx.viewportHovered && !gizmoBusy && !ctx.flying && ctx.camera && ctx.scene && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_Pressing = true;
            m_Boxing = false;
            m_PressPos = ImGui::GetMousePos();
        }
        if (m_Pressing)
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            if (ctx.flying || !ctx.camera || !ctx.scene)
                m_Pressing = false;
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                const float dx = mouse.x - m_PressPos.x, dy = mouse.y - m_PressPos.y;
                if (dx * dx + dy * dy > 16.0f * ImGui::GetIO().FontGlobalScale * ImGui::GetIO().FontGlobalScale) m_Boxing = true;
                if (m_Boxing)                                                  // the box, clipped to the picture
                {
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    dl->PushClipRect(ImVec2(imageMin.x, imageMin.y), ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), true);
                    dl->AddRectFilled(m_PressPos, mouse, IM_COL32(255, 166, 26, 38));
                    dl->AddRect(m_PressPos, mouse, IM_COL32(255, 166, 26, 220), 0.0f, 0, 1.0f);
                    dl->PopClipRect();
                }
            }
            else
            {
                m_Pressing = false;
                FinishClickOrBox(ctx, mouse, imageMin.x, imageMin.y, imageSize.x, imageSize.y);
            }
        }

        if (ctx.viewportFocused && !ctx.flying && !ImGui::GetIO().WantTextInput)
            HandleShortcuts(ctx);

        ImGui::End();
    }

    void ViewportPanel::FinishClickOrBox(EditorContext& ctx, const ImVec2& mouse, float imageX, float imageY, float imageW, float imageH)
    {
        const ImGuiIO& io = ImGui::GetIO();
        const bool adding = io.KeyCtrl || io.KeyShift;       // Ctrl / Shift: the result joins the selection (Ctrl: a click on a selected one removes it)

        if (m_Boxing)
        {
            Camera& camera = *ctx.camera->GetCamera();
            auto inImage = [&](const ImVec2& p) { return ImVec2(std::clamp(p.x - imageX, 0.0f, imageW), std::clamp(p.y - imageY, 0.0f, imageH)); };
            const ImVec2 a = inImage(m_PressPos), b = inImage(mouse);
            const std::vector<Entity> inside = EntitiesInRect(*ctx.scene, camera.GetViewMatrix4X4(), camera.GetProjection4X4(), imageW, imageH, a.x, a.y, b.x, b.y);
            if (adding)
                for (const Entity& e : inside) ctx.selection.Add(e);
            else
                ctx.selection.SetMany(inside);
            if (!inside.empty()) ctx.revealSelection = true;
            m_Boxing = false;
            return;
        }

        // a click: what is under the press
        float x = 0, y = 0;
        if (!ViewportMath::ScreenToTarget(m_PressPos.x, m_PressPos.y, imageX, imageY, imageW, imageH, m_Size.width, m_Size.height, x, y))
            return;
        const PickResult hit = ScenePicking::Pick(*ctx.scene, *ctx.camera->GetCamera(), x, y, m_Size.width, m_Size.height);
        if (hit)
        {
            if (io.KeyCtrl)       ctx.selection.Toggle(hit.entity);
            else if (io.KeyShift) ctx.selection.Add(hit.entity);
            else                  ctx.selection.Set(hit.entity);
            ctx.revealSelection = true;
        }
        else if (!adding)
            ctx.selection.Clear();
    }

    void ViewportPanel::DrawFlyHud(EditorContext& ctx, float imageX, float imageY, float imageW)
    {
        if (!ctx.flying || !ctx.camera) return;
        char text[96];
        snprintf(text, sizeof(text), "Fly speed %.1f   (mouse wheel to change)", ctx.camera->GetMoveSpeed());
        const ImVec2 size = ImGui::CalcTextSize(text);
        const float pad = 8.0f;
        const ImVec2 min(imageX + (imageW - size.x) * 0.5f - pad, imageY + 12.0f);
        const ImVec2 max(min.x + size.x + pad * 2.0f, min.y + size.y + pad);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(min, max, IM_COL32(12, 14, 20, 200), 6.0f);
        dl->AddText(ImVec2(min.x + pad, min.y + pad * 0.5f), IM_COL32(230, 232, 238, 255), text);
    }

    void ViewportPanel::DrawSelectionOutline(EditorContext& ctx, float imageX, float imageY, float imageW, float imageH)
    {
        using namespace DirectX;
        const std::vector<Entity> selected = ctx.selection.All();
        if (selected.empty() || imageW < 1.0f || imageH < 1.0f) return;

        Camera& camera = *ctx.camera->GetCamera();
        const XMFLOAT4X4 view = camera.GetViewMatrix4X4();
        const XMFLOAT4X4 proj = camera.GetProjection4X4();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(ImVec2(imageX, imageY), ImVec2(imageX + imageW, imageY + imageH), true);

        auto project = [&](const XMFLOAT3& p, ImVec2& out)
        {
            XMFLOAT2 s;
            if (!ViewportMath::ProjectToViewport(p, view, proj, imageW, imageH, s)) return false;
            out = ImVec2(imageX + s.x, imageY + s.y);
            return true;
        };
        auto line = [&](const XMFLOAT3& a, const XMFLOAT3& b, ImU32 color, float thickness)
        {
            ImVec2 pa, pb;
            if (project(a, pa) && project(b, pb)) dl->AddLine(pa, pb, color, thickness);
        };

        for (Entity e : selected)
        {
            const ImU32 outline = ctx.selection.IsPrimary(e) ? kOutline : kOutlineOthers;
            const EntityBounds bounds = ComputeEntityBounds(e);
            if (bounds.hasBox)
            {
                for (int i = 0; i < 8; ++i)                       // 12 edges: corners that differ in exactly one bit
                    for (int bit : { 1, 2, 4 })
                        if (!(i & bit))
                            line(bounds.corners[i], bounds.corners[i | bit], outline, 1.5f);
            }
            else                                                   // no mesh: axes at the position (+ the aim of a light)
            {
                const XMFLOAT3 c = bounds.center;
                const float len = bounds.radius;
                line(c, { c.x + len, c.y, c.z }, IM_COL32(230, 60, 60, 255), 2.0f);
                line(c, { c.x, c.y + len, c.z }, IM_COL32(60, 200, 60, 255), 2.0f);
                line(c, { c.x, c.y, c.z + len }, IM_COL32(70, 110, 240, 255), 2.0f);
                if (const LightComponent* light = e.TryGet<LightComponent>())
                    if (light->type != LightType::Point)
                    {
                        const XMFLOAT3 f = e.Get<TransformComponent>().WorldForward();
                        line(c, { c.x + f.x * 2.0f, c.y + f.y * 2.0f, c.z + f.z * 2.0f }, IM_COL32(255, 230, 80, 255), 2.0f);
                    }
                ImVec2 p;
                if (project(c, p)) dl->AddCircle(p, 6.0f, outline, 16, 1.5f);
            }
        }
        dl->PopClipRect();
    }

    void ViewportPanel::HandleShortcuts(EditorContext& ctx)
    {
        if (ctx.gizmo->IsAvailable())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) ctx.gizmoSettings.operation = GizmoOperation::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_E, false)) ctx.gizmoSettings.operation = GizmoOperation::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) ctx.gizmoSettings.operation = GizmoOperation::Scale;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) ctx.gizmoSettings.operation = GizmoOperation::None;
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) FrameSelection(ctx);

        if (!ctx.selection.Has() || !ctx.scene) return;
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            SceneEditing::DestroyEntities(*ctx.scene, ctx.selection.All());
            ctx.MarkEdited();
        }
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && SceneEditing::CanDuplicateAny(*ctx.scene, ctx.selection.All()))
        {
            ctx.MarkEdited();
            const std::vector<Entity> copies = SceneEditing::DuplicateEntities(*ctx.scene, ctx.selection.All());
            if (!copies.empty())
            {
                ctx.selection.SetMany(copies);
                ctx.revealSelection = true;
            }
        }
    }

    void ViewportPanel::FrameSelection(EditorContext& ctx)
    {
        using namespace DirectX;
        if (!ctx.camera || !ctx.selection.Has()) return;

        const EntityBounds b = ComputeSelectionBounds(ctx.selection.All());     // all of it, not just the primary
        Camera& camera = *ctx.camera->GetCamera();
        const XMFLOAT3 forward = camera.GetForwardVector();         // keep the way the camera looks, back off along it
        const float distance = std::max(b.radius * 3.0f, 2.0f);
        camera.SetPosition({ b.center.x - forward.x * distance, b.center.y - forward.y * distance, b.center.z - forward.z * distance });
    }
}
