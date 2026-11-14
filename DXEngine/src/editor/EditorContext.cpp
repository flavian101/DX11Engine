#include "dxpch.h"
#include "EditorContext.h"
#include "SceneEditing.h"
#include "EditorIcons.h"
#include "EditorTheme.h"

#include "imgui.h"

namespace DXEngine
{
    namespace
    {
        // a menu item with an icon in front of its label (drawn after the item, so the hover highlight does not cover it)
        bool IconItem(Icon icon, const ImVec4& color, const char* label)
        {
            const float h = ImGui::GetFrameHeight();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            char padded[128];
            snprintf(padded, sizeof(padded), "      %s", label);          // room for the icon
            const bool clicked = ImGui::MenuItem(padded);
            EditorIcons::Draw(ImGui::GetWindowDrawList(), icon, ImVec2(p.x + h * 0.55f, p.y + h * 0.5f), h * 0.62f,
                              ImGui::ColorConvertFloat4ToU32(color));
            return clicked;
        }
    }

    void DrawCreateMenu(EditorContext& ctx, Entity parent)
    {
        if (!ctx.scene) return;
        Scene& scene = *ctx.scene;
        const EditorTheme::Palette& pal = EditorTheme::Colors();
        const bool underParent = parent.IsValid();

        auto place = [&](Entity e)
        {
            if (!e.IsValid()) return;
            // a child appears at its parent's origin; a root where the camera is looking
            if (!underParent)
                e.Get<TransformComponent>().position = ctx.SpawnPoint();
            ctx.selection.Set(e);
            ctx.revealSelection = true;
        };

        if (IconItem(Icon::Entity, pal.kindEmpty, "Empty"))
            place(SceneEditing::CreateEmpty(scene, "Empty", parent));

        ImGui::SeparatorText("3D object");
        ImGui::BeginDisabled(!ctx.device);                    // a mesh needs the GPU
        if (IconItem(Icon::Mesh, pal.kindMesh, "Cube"))   place(SceneEditing::CreatePrimitive(scene, ctx.device, SceneEditing::Primitive::Cube, parent));
        if (IconItem(Icon::Mesh, pal.kindMesh, "Sphere")) place(SceneEditing::CreatePrimitive(scene, ctx.device, SceneEditing::Primitive::Sphere, parent));
        if (IconItem(Icon::Mesh, pal.kindMesh, "Plane"))  place(SceneEditing::CreatePrimitive(scene, ctx.device, SceneEditing::Primitive::Plane, parent));
        ImGui::EndDisabled();

        ImGui::SeparatorText("Light");
        if (IconItem(Icon::Light, pal.kindLight, "Point light"))       place(SceneEditing::CreateLight(scene, LightType::Point, parent));
        if (IconItem(Icon::Spot,  pal.kindLight, "Spot light"))        place(SceneEditing::CreateLight(scene, LightType::Spot, parent));
        if (IconItem(Icon::Sun,   pal.kindLight, "Directional light")) place(SceneEditing::CreateLight(scene, LightType::Directional, parent));
    }
}
