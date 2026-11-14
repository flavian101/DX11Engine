#include "dxpch.h"
#include "ImGuizmoGizmo.h"

#include "imgui.h"
#include "ImGuizmo.h"

namespace DXEngine
{
    bool ImGuizmoGizmo::Manipulate(const GizmoView& view, const GizmoSettings& settings, DirectX::XMFLOAT4X4& world)
    {
        if (settings.operation == GizmoOperation::None || view.width < 1.0f || view.height < 1.0f)
            return false;

        // ImGuizmo needs a BeginFrame once per ImGui frame; do it here (the first call of a frame) so no other code has to know.
        static int lastFrame = -1;
        if (ImGui::GetFrameCount() != lastFrame)
        {
            lastFrame = ImGui::GetFrameCount();
            ImGuizmo::BeginFrame();
        }

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(view.x, view.y, view.width, view.height);

        ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
        if (settings.operation == GizmoOperation::Rotate) operation = ImGuizmo::ROTATE;
        if (settings.operation == GizmoOperation::Scale)  operation = ImGuizmo::SCALE;

        const ImGuizmo::MODE mode = (settings.operation == GizmoOperation::Scale || settings.space == GizmoSpace::Local)
                                  ? ImGuizmo::LOCAL        // scaling is only meaningful along the object's own axes
                                  : ImGuizmo::WORLD;

        const float step = settings.SnapStep();
        const float snap[3] = { step, step, step };

        return ImGuizmo::Manipulate(&view.view._11, &view.projection._11, operation, mode, &world._11,
                                    nullptr, step > 0.0f ? snap : nullptr);
    }

    bool ImGuizmoGizmo::IsUsing() const { return ImGuizmo::IsUsing(); }
    bool ImGuizmoGizmo::IsOver() const  { return ImGuizmo::IsOver(); }
}
