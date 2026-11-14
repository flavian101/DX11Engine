#include "dxpch.h"
#include "EditorChrome.h"
#include "EditorIcons.h"
#include "EditorTheme.h"
#include "renderer/Renderer.h"
#include "renderer/Core/RenderTypes.h"

#include "imgui.h"
#include "imgui_internal.h"      // BeginViewportSideBar, SeparatorEx

namespace DXEngine
{
    namespace EditorChrome
    {
        void FormatStats(char* out, size_t size, float fps, size_t entities, unsigned drawCalls, unsigned triangles)
        {
            auto thousands = [](unsigned v, char* buf, size_t n)       // 1234567 -> "1,234,567"
            {
                char raw[32];
                snprintf(raw, sizeof(raw), "%u", v);
                const size_t len = strlen(raw);
                size_t o = 0;
                for (size_t i = 0; i < len && o + 2 < n; ++i)
                {
                    buf[o++] = raw[i];
                    const size_t left = len - 1 - i;
                    if (left > 0 && left % 3 == 0) buf[o++] = ',';
                }
                buf[o] = '\0';
            };
            char tris[32];
            thousands(triangles, tris, sizeof(tris));
            snprintf(out, size, "%.0f FPS    %zu entities    %u draws    %s tris", fps, entities, drawCalls, tris);
        }

        namespace
        {
            void Divider()
            {
                ImGui::SameLine();
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 1.0f);
                ImGui::SameLine();
            }
        }

        ToolbarResult DrawToolbar(EditorContext& ctx)
        {
            ToolbarResult result;
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            const float scale = EditorTheme::Scale();
            const float height = ImGui::GetFrameHeight() + 14.0f * scale;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * scale, 7.0f * scale));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.125f, 0.132f, 0.152f, 1.0f));
            const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;
            if (ImGui::BeginViewportSideBar("##EditorToolbar", viewport, ImGuiDir_Up, height, flags))
            {
                GizmoSettings& g = ctx.gizmoSettings;
                const bool gizmo = ctx.gizmo && ctx.gizmo->IsAvailable();

                // ---- tools
                if (EditorIcons::Button("##select", Icon::Select, g.operation == GizmoOperation::None, "Select  (Q)"))
                    g.operation = GizmoOperation::None;
                ImGui::SameLine();
                ImGui::BeginDisabled(!gizmo);
                if (EditorIcons::Button("##move", Icon::Move, g.operation == GizmoOperation::Translate, "Move  (W)"))
                    g.operation = GizmoOperation::Translate;
                ImGui::SameLine();
                if (EditorIcons::Button("##rotate", Icon::Rotate, g.operation == GizmoOperation::Rotate, "Rotate  (E)"))
                    g.operation = GizmoOperation::Rotate;
                ImGui::SameLine();
                if (EditorIcons::Button("##scale", Icon::Scale, g.operation == GizmoOperation::Scale, "Scale  (R)"))
                    g.operation = GizmoOperation::Scale;

                // ---- space and snapping
                Divider();
                const bool world = g.space == GizmoSpace::World;
                if (ImGui::Button(world ? "World" : "Local", ImVec2(62.0f * scale, 0.0f)))
                    g.space = world ? GizmoSpace::Local : GizmoSpace::World;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Gizmo space: world axes or the object's own axes\n(scaling always uses the object's axes)");
                ImGui::SameLine();
                ImGui::Checkbox("Snap", &g.snap);
                if (g.snap)
                {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(84.0f * scale);
                    switch (g.operation)
                    {
                    case GizmoOperation::Rotate: ImGui::DragFloat("##step", &g.rotateSnapDegrees, 0.5f, 1.0f, 90.0f, "%.0f deg"); break;
                    case GizmoOperation::Scale:  ImGui::DragFloat("##step", &g.scaleSnap, 0.01f, 0.01f, 10.0f, "%.2f"); break;
                    default:                     ImGui::DragFloat("##step", &g.translateSnap, 0.05f, 0.01f, 100.0f, "%.2f m"); break;
                    }
                }
                ImGui::EndDisabled();
                if (!gizmo)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(no gizmo)");
                }

                // ---- right side: display switches and play
                const float buttons = ImGui::GetFrameHeight();
                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                const float group = buttons * 3.0f + spacing * 5.0f + 1.0f;
                ImGui::SameLine(ImGui::GetWindowWidth() - group - ImGui::GetStyle().WindowPadding.x);

                bool wireframe = Renderer::IsWireframeEnabled();
                if (EditorIcons::Button("##wire", Icon::Wireframe, wireframe, "Wireframe"))
                    Renderer::EnableWireframe(!wireframe);
                ImGui::SameLine();
                RenderEnvironment env = Renderer::GetEnvironment();
                if (EditorIcons::Button("##shadows", Icon::Shadow, env.shadows.enabled, "Shadows"))
                {
                    env.shadows.enabled = !env.shadows.enabled;
                    Renderer::SetEnvironment(env);
                }
                Divider();
                if (EditorIcons::Button("##play", Icon::Play, false, "Play: the scene fills the window  (F1 to come back)"))
                    result.togglePlay = true;

                ImGui::End();
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            return result;
        }

        void DrawStatusBar(EditorContext& ctx, const char* hint)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            const float scale = EditorTheme::Scale();
            const EditorTheme::Palette& pal = EditorTheme::Colors();
            const float height = ImGui::GetFrameHeight() + 4.0f * scale;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scale, 3.0f * scale));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.075f, 0.080f, 0.095f, 1.0f));
            const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;
            if (ImGui::BeginViewportSideBar("##EditorStatus", viewport, ImGuiDir_Down, height, flags))
            {
                ImGui::AlignTextToFramePadding();

                // what is selected
                Entity e = ctx.selection.Get();
                if (e.IsValid())
                {
                    ImGui::TextUnformatted(e.Get<NameComponent>().name.c_str());
                    ImGui::SameLine();
                    ImGui::TextColored(pal.textDim, "%s%s%s",
                                       e.Has<MeshRendererComponent>() ? " mesh" : "",
                                       e.Has<LightComponent>() ? " light" : "",
                                       e.Has<SkyboxComponent>() ? " sky" : "");
                }
                else
                    ImGui::TextColored(pal.textDim, "Nothing selected");

                // a hint for what the mouse is over
                if (hint && hint[0])
                {
                    ImGui::SameLine(0.0f, 28.0f * scale);
                    ImGui::TextColored(pal.textDim, "%s", hint);
                }

                // the numbers, right aligned
                char stats[160];
                const RenderStatistics& st = Renderer::GetStats();
                FormatStats(stats, sizeof(stats), ImGui::GetIO().Framerate, ctx.scene ? ctx.scene->EntityCount() : 0, st.drawCalls, st.trianglesRendered);
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(stats).x - ImGui::GetStyle().WindowPadding.x);
                ImGui::TextColored(pal.textDim, "%s", stats);

                ImGui::End();
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
    }
}
