#include "dxpch.h"
#include "EditorContext.h"
#include "SceneEditing.h"

#include "imgui.h"

namespace DXEngine
{
    void DrawCreateMenu(EditorContext& ctx, Entity parent)
    {
        if (!ctx.scene) return;
        Scene& scene = *ctx.scene;
        auto select = [&](Entity e)
        {
            if (!e.IsValid()) return;
            ctx.selection.Set(e);
            ctx.revealSelection = true;
        };

        if (ImGui::MenuItem("Empty"))             select(SceneEditing::CreateEmpty(scene, "Empty", parent));
        ImGui::Separator();
        if (ImGui::MenuItem("Point light"))       select(SceneEditing::CreateLight(scene, LightType::Point, parent));
        if (ImGui::MenuItem("Spot light"))        select(SceneEditing::CreateLight(scene, LightType::Spot, parent));
        if (ImGui::MenuItem("Directional light")) select(SceneEditing::CreateLight(scene, LightType::Directional, parent));
    }
}
