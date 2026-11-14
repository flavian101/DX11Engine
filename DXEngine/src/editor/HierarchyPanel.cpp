#include "dxpch.h"
#include "HierarchyPanel.h"
#include "EditorIcons.h"
#include "EditorTheme.h"
#include "SceneEditing.h"

#include "imgui.h"
#include "imgui_internal.h"      // RenderTextClipped

namespace DXEngine
{
    namespace
    {
        const char* kPayload = "DX_ENTITY";

        EditorIcons::Kind KindOf(Entity e)
        {
            const EditorTheme::Palette& p = EditorTheme::Colors();
            if (const LightComponent* l = e.TryGet<LightComponent>())
                return { l->type == LightType::Directional ? Icon::Sun : l->type == LightType::Spot ? Icon::Spot : Icon::Light, p.kindLight };
            if (e.Has<SkyboxComponent>())       return { Icon::Sky, p.kindSky };
            if (e.Has<MeshRendererComponent>()) return { Icon::Mesh, p.kindMesh };
            return { Icon::Entity, p.kindEmpty };
        }

        Entity FromPayload(Scene& scene, const ImGuiPayload* payload)
        {
            if (!payload || payload->DataSize != sizeof(uint32_t)) return Entity();
            const entt::entity id = static_cast<entt::entity>(*static_cast<const uint32_t*>(payload->Data));
            return scene.Registry().valid(id) ? scene.Wrap(id) : Entity();
        }
    }

    void HierarchyPanel::BeginRename(Entity e)
    {
        if (!e.IsValid()) return;
        m_RenameEntity = e;
        snprintf(m_RenameBuffer, sizeof(m_RenameBuffer), "%s", e.Get<NameComponent>().name.c_str());
        m_RenameFocus = true;
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
        const EditorTheme::Palette& pal = EditorTheme::Colors();

        // ---- top bar: search (with a clear button) and add ----
        const float button = ImGui::GetFrameHeight();
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - button - spacing);
            ImGui::InputTextWithHint("##filter", "Search...", m_Filter, sizeof(m_Filter));
            const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
            if (m_Filter[0])      // a clear button inside the field's right edge
            {
                ImGui::SetCursorScreenPos(ImVec2(max.x - button, min.y));
                if (EditorIcons::Button("##clearfilter", Icon::Close, false, "Clear", button))
                    m_Filter[0] = '\0';
                ImGui::SetCursorScreenPos(ImVec2(max.x + spacing, min.y));
            }
            else
            {
                EditorIcons::Draw(ImGui::GetWindowDrawList(), Icon::Search, ImVec2(max.x - button * 0.5f, (min.y + max.y) * 0.5f), button * 0.5f,
                                  ImGui::ColorConvertFloat4ToU32(pal.textDim));
                ImGui::SameLine();
            }
            if (EditorIcons::Button("##add", Icon::Plus, false, "Create", button))
                ImGui::OpenPopup("##create");
            if (ImGui::BeginPopup("##create"))
            {
                DrawCreateMenu(ctx, Entity());
                ImGui::EndPopup();
            }
        }
        ImGui::Spacing();

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
        const float rowHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;

        if (rows.empty())
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, pal.textDim);
            ImGui::TextWrapped(m_Model.IsFiltering() ? "Nothing is called like that." : "The scene is empty.\nUse + or right click to create something.");
            ImGui::PopStyleColor();
        }

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

        // the empty area below the rows: a click clears the selection, a drop makes the entity a root, right click creates
        ImVec2 rest = ImGui::GetContentRegionAvail();
        rest.y = std::max(rest.y, 32.0f);
        ImGui::InvisibleButton("##empty", rest);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            ctx.selection.Clear();
        if (ImGui::BeginPopupContextItem("##emptyctx"))
        {
            DrawCreateMenu(ctx, Entity());
            ImGui::EndPopup();
        }
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
            if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
                BeginRename(ctx.selection.Get());
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && SceneEditing::CanDuplicate(scene, ctx.selection.Get()))
                m_PendingDuplicate = ctx.selection.Get();
        }

        ImGui::End();
        ApplyPending(ctx);
    }

    void HierarchyPanel::DrawRow(EditorContext& ctx, const HierarchyRow& row)
    {
        Scene& scene = *ctx.scene;
        Entity e = row.entity;
        const EditorTheme::Palette& pal = EditorTheme::Colors();

        ImGui::PushID(static_cast<int>(entt::to_integral(e.Id())));

        const float rowH = ImGui::GetFrameHeight();
        const float indent = m_Model.IsFiltering() ? 0.0f : static_cast<float>(row.depth) * rowH * 0.8f;
        const float eyeW = rowH;
        const bool hasRenderer = e.Has<MeshRendererComponent>();
        const bool selected = ctx.selection.Is(e);

        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float rowW = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // 1. the arrow (or nothing, keeping the names aligned)
        ImGui::Dummy(ImVec2(indent, rowH));
        ImGui::SameLine(0.0f, 0.0f);
        const ImVec2 arrowPos = ImGui::GetCursorScreenPos();
        bool arrowHit = false;
        if (row.hasChildren && !m_Model.IsFiltering())
            arrowHit = ImGui::InvisibleButton("##arrow", ImVec2(rowH, rowH));
        else
            ImGui::Dummy(ImVec2(rowH, rowH));
        ImGui::SameLine(0.0f, 0.0f);
        if (arrowHit)
            m_Model.Toggle(e);

        // 2. the row: one button for selecting / dragging / dropping / the context menu
        const float mainW = std::max(1.0f, rowW - indent - rowH - (hasRenderer ? eyeW : 0.0f));
        const ImVec2 mainPos = ImGui::GetCursorScreenPos();
        const bool renaming = m_RenameEntity == e;

        // background first, so the text drawn later is on top of it
        const ImVec2 bgMax(rowMin.x + rowW, rowMin.y + rowH);
        bool hovered = false, clicked = false;
        if (!renaming)
        {
            clicked = ImGui::InvisibleButton("##row", ImVec2(mainW, rowH));
            hovered = ImGui::IsItemHovered();
        }
        if (selected)
            dl->AddRectFilled(rowMin, bgMax, ImGui::ColorConvertFloat4ToU32(ImVec4(pal.accent.x, pal.accent.y, pal.accent.z, 0.30f)), 3.0f);
        else if (hovered)
            dl->AddRectFilled(rowMin, bgMax, IM_COL32(255, 255, 255, 14), 3.0f);

        // arrow glyph
        if (row.hasChildren && !m_Model.IsFiltering())
            ImGui::RenderArrow(dl, ImVec2(arrowPos.x + rowH * 0.2f, arrowPos.y + rowH * 0.2f), ImGui::GetColorU32(ImGuiCol_Text),
                               row.expanded ? ImGuiDir_Down : ImGuiDir_Right, 0.6f);

        // icon, then the name
        const EditorIcons::Kind kind = KindOf(e);
        const float iconSize = rowH * 0.62f;
        EditorIcons::Draw(dl, kind.icon, ImVec2(mainPos.x + rowH * 0.5f, mainPos.y + rowH * 0.5f), iconSize, ImGui::ColorConvertFloat4ToU32(kind.color));

        const NameComponent* nameComponent = e.TryGet<NameComponent>();
        const char* name = nameComponent ? nameComponent->name.c_str() : "(unnamed)";
        const bool hidden = hasRenderer && !e.Get<MeshRendererComponent>().visible;
        const ImVec2 textMin(mainPos.x + rowH + 2.0f, mainPos.y);

        if (renaming)
        {
            ImGui::SetCursorScreenPos(textMin);
            ImGui::SetNextItemWidth(std::max(1.0f, mainW - rowH - 2.0f));
            if (m_RenameFocus) { ImGui::SetKeyboardFocusHere(); m_RenameFocus = false; }
            const bool entered = ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer),
                                                  ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            const bool escaped = ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
            if (entered && m_RenameBuffer[0])
                e.Get<NameComponent>().name = m_RenameBuffer;
            if (entered || escaped || (!ImGui::IsItemActive() && !m_RenameFocus && ImGui::IsItemDeactivated()))
                m_RenameEntity = Entity();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, hidden ? pal.textDim : pal.text);
            ImGui::RenderTextClipped(textMin, ImVec2(mainPos.x + mainW, mainPos.y + rowH), name, nullptr, nullptr, ImVec2(0.0f, 0.5f));
            ImGui::PopStyleColor();
        }

        // 3. select / rename / expand
        if (clicked)
            ctx.selection.Set(e);
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (row.hasChildren) m_Model.Toggle(e);
            else                 BeginRename(e);
        }
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            ctx.selection.Set(e);

        // 4. drag it onto another entity to make it a child
        if (!renaming && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            const uint32_t id = entt::to_integral(e.Id());
            ImGui::SetDragDropPayload(kPayload, &id, sizeof(id));
            ImGui::Text("%s", name);
            ImGui::EndDragDropSource();
        }
        if (!renaming && ImGui::BeginDragDropTarget())
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

        // 5. the context menu
        if (!renaming && ImGui::BeginPopupContextItem("##ctx", ImGuiPopupFlags_MouseButtonRight))
        {
            if (ImGui::BeginMenu("Create child"))
            {
                DrawCreateMenu(ctx, e);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Rename", "F2"))
                BeginRename(e);
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

        // 6. the eye: show / hide a mesh
        if (hasRenderer)
        {
            ImGui::SetCursorScreenPos(ImVec2(rowMin.x + rowW - eyeW, rowMin.y));
            MeshRendererComponent& mr = e.Get<MeshRendererComponent>();
            if (EditorIcons::Button("##eye", mr.visible ? Icon::Eye : Icon::EyeOff, false, mr.visible ? "Hide" : "Show", rowH))
                mr.visible = !mr.visible;
        }

        // The next row starts exactly one row height below this one, whatever the widgets above did to the cursor
        // (the list clipper relies on every row having the same height).
        ImGui::SetCursorScreenPos(rowMin);
        ImGui::Dummy(ImVec2(rowW, rowH));
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
