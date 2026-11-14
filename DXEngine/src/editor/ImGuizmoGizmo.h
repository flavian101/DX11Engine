#pragma once
#include "GizmoTypes.h"

namespace DXEngine
{
    // ==================================================================
    // ImGuizmo behind IGizmo (see GizmoTypes.h). The only place that knows ImGuizmo exists.
    //
    // Conventions: ImGuizmo reads a matrix as 16 floats laid out like DirectXMath's row-vector matrices (right, up, forward,
    // position in the four rows), and works with Direct3D's 0..1 clip depth, so the engine's matrices go in as they are.
    // Handedness is not an option of ImGuizmo; editor_ui_tests drives it with the engine's left-handed camera and checks
    // that dragging a handle moves the entity along the axis that was dragged, in the direction it was dragged.
    // ==================================================================
    class ImGuizmoGizmo final : public IGizmo
    {
    public:
        const char* Name() const override { return "ImGuizmo"; }
        bool IsAvailable() const override { return true; }

        // Call inside the viewport window (it draws into that window's draw list).
        bool Manipulate(const GizmoView& view, const GizmoSettings& settings, DirectX::XMFLOAT4X4& world) override;

        bool IsUsing() const override;
        bool IsOver() const override;
    };
}
