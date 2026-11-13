#include "dxpch.h"
#include "HierarchyPanel.h"
#include "SceneEditing.h"

#include "imgui.h"
#include "imgui_internal.h"      // RenderArrow

namespace DXEngine
{
    namespace
    {
        const char* kPayload = "DX_ENTITY";

        std::string EntityLabel(Entity e)
        {
            const char* kind = e.Has<LightComponent>()        ? "[L] "
                             : e.Has<SkyboxComponent>()       ? "[S] "
                             : e.Has<MeshRendererComponent>() ? "[M] "
                                                              : "    ";
            const NameComponent* name = e.TryGet<NameComponent>();
            return std::string(kind) + (name ? name->name : std::string("(unnamed)"));
        }

        Entity FromPayload(Scene& scene, const ImGuiPayload* payload)
        {
            if (!payload || payload->DataSize != sizeof(uint32_t)) return Entity();
            const entt::entity id = static_cast<entt::entity>(*static_cast<const uint32_t*>(payload->Data));
            return scene.Registry().valid(id) ? scene.Wrap(id) : Entity();
        }
    }

    void HierarchyPanel::OnImGui(EditorContext& ctx, bool* open)
    {
        if (open && !*open) return;
        if (!ImGui::Begin("Hierarchy", open))
        {
            ImGui::End();
            return;
        }
        if (!ctx.scene)
        {
            ImGui::TextDisabled("No scene");
            ImGui::End();
            return;
        }
        Scene& scene = *ctx.scene;

        // ---- top bar: create + search ----
        if (ImGui::Button("+"))
            ImGui::OpenPopup("##create");
        if (ImGui::BeginPopup("##create"))
        {
            DrawCreateMenu(ctx, Entity());
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", "Search", m_Filter, sizeof(m_Filter));

        // a pick in the viewport selected something: open the chain above it (and drop the filter that may hide it)
        if (ctx.revealSelection)
        {
            ctx.revealSelection = false;
            if (ctx.selection.Has())
            {
                m_Filter[0] = '\0';
                m_Model.RevealEntity(scene, ctx.selection.Get());
                m_ScrollToSelection = true;
            }
        }
        m_Model.Rebuild(scene, m_Filter);

        // ---- the rows ----
        ImGui::BeginChild("##rows", ImVec2(0, 0), ImGuiChildFlags_None);

        const std::vector<HierarchyRow>& rows = m_Model.Rows();
        const float rowHeight = ImGui::GetTextLineHeightWithSpacing();

        if (m_ScrollToSelection)
        {
            m_ScrollToSelection = false;
            for (size_t i = 0; i < rows.size(); ++i)
                if (ctx.selection.Is(rows[i].entity))
                {
                    ImGui::SetScrollY(std::max(0.0f, static_cast<float>(i) * rowHeight - ImGui::GetWindowHeight() * 0.5f));
                    break;
                }
        }

        ImGuiListClipper clipper;                      // only the rows on screen are drawn: 2000 entities cost like 30
        clipper.Begin(static_cast<int>(rows.size()), rowHeight);
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                DrawRow(ctx, rows[static_cast<size_t>(i)]);
        clipper.End();

        // the empty area below the rows: a click clears the selection, a drop makes the entity a root
        ImVec2 rest = ImGui::GetContentRegionAvail();
        rest.y = std::max(rest.y, 32.0f);
        ImGui::InvisibleButton("##empty", rest);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            ctx.selection.Clear();
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayload))
            {
                Entity dragged = FromPayload(scene, p);
                if (dragged.IsValid() && scene.GetParent(dragged).IsValid())
                {
                    m_PendingReparentChild = dragged;
                    m_PendingReparentParent = Entity();
                    m_HasPendingReparent = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::EndChild();

        // ---- keyboard, while this window has focus and no text field is being edited ----
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput && ctx.selection.Has())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                m_PendingDelete = ctx.selection.Get();
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && SceneEditing::CanDuplicate(scene, ctx.selection.Get()))
                m_PendingDuplicate = ctx.selection.Get();
        }

        ImGui::End();
        ApplyPending(ctx);
    }

    void HierarchyPanel::DrawRow(EditorContext& ctx, const HierarchyRow& row)
    {
        Scene& scene = *ctx.scene;
        const Entity e = row.entity;

        ImGui::PushID(static_cast<int>(entt::to_integral(e.Id())));

        const float lineHeight = ImGui::GetTextLineHeight();
        if (!m_Model.IsFiltering())
            ImGui::Indent(static_cast<float>(row.depth) * 16.0f);

        // expand arrow (or a spacer of the same width, so names line up)
        if (row.hasChildren && !m_Model.IsFiltering())
        {
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##arrow", ImVec2(lineHeight, lineHeight)))
                m_Model.Toggle(e);
            ImGui::RenderArrow(ImGui::GetWindowDrawList(), pos, ImGui::GetColorU32(ImGuiCol_Text),
                               row.expanded ? ImGuiDir_Down : ImGuiDir_Right, 0.8f);
        }
        else
            ImGui::Dummy(ImVec2(lineHeight, lineHeight));
        ImGui::SameLine(0.0f, 2.0f);

        // the entity itself
        const std::string label = EntityLabel(e);
        if (ImGui::Selectable(label.c_str(), ctx.selection.Is(e), ImGuiSelectableFlags_AllowDoubleClick))
        {
            ctx.selection.Set(e);
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && row.hasChildren)
                m_Model.Toggle(e);
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ctx.selection.Set(e);

        // drag it onto another entity to make it a child
        if (ImGui::BeginDragDropSource())
        {
            const uint32_t id = entt::to_integral(e.Id());
            ImGui::SetDragDropPayload(kPayload, &id, sizeof(id));
            ImGui::TextUnformatted(label.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kPayload))
            {
                Entity dragged = FromPayload(scene, p);
                if (dragged.IsValid() && dragged != e)
                {
                    m_PendingReparentChild = dragged;
                    m_PendingReparentParent = e;
                    m_HasPendingReparent = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("##ctx"))
        {
            if (ImGui::BeginMenu("Create child"))
            {
                DrawCreateMenu(ctx, e);
                ImGui::EndMenu();
            }
            const bool canDuplicate = SceneEditing::CanDuplicate(scene, e);
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, canDuplicate))
                m_PendingDuplicate = e;
            if (!canDuplicate && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Skinned meshes, animations and the sky cannot be duplicated yet");
            if (ImGui::MenuItem("Unparent", nullptr, false, scene.GetParent(e).IsValid()))
            {
                m_PendingReparentChild = e;
                m_PendingReparentParent = Entity();
                m_HasPendingReparent = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del"))
                m_PendingDelete = e;
            ImGui::EndPopup();
        }

        if (!m_Model.IsFiltering())
            ImGui::Unindent(static_cast<float>(row.depth) * 16.0f);
        ImGui::PopID();
    }

    void HierarchyPanel::ApplyPending(EditorContext& ctx)
    {
        Scene& scene = *ctx.scene;

        if (m_HasPendingReparent)
        {
            m_HasPendingReparent = false;
            if (SceneEditing::Reparent(scene, m_PendingReparentChild, m_PendingReparentParent) && m_PendingReparentParent.IsValid())
                m_Model.SetExpanded(m_PendingReparentParent, true);       // show where it went
        }
        if (m_PendingDuplicate.IsValid())
        {
            Entity copy = SceneEditing::Duplicate(scene, m_PendingDuplicate);
            if (copy.IsValid())
            {
                ctx.selection.Set(copy);
                ctx.revealSelection = true;
            }
        }
        if (m_PendingDelete.IsValid())
            scene.DestroyEntity(m_PendingDelete);          // its descendants go with it; the selection reads as empty by itself

        m_PendingDelete = m_PendingDuplicate = m_PendingReparentChild = m_PendingReparentParent = Entity();
    }
}
