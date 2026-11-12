#pragma once
#include <DirectXMath.h>
#include <cmath>
#include <cstdint>

namespace DXEngine
{
    // ==================================================================
    // Gizmo foundations. The editor talks to a gizmo only through IGizmo, so the actual widget (ImGuizmo, or anything else)
    // is a drop-in: nothing in the editor changes when one is plugged in. Until then NullGizmo is installed, the tool
    // buttons say so, and everything else (selection, outline, inspector) works.
    //
    // What the editor already does around the gizmo, so a backend only has to draw and drag:
    //  * the tool (W / E / R) and the space (local / world) and the snapping settings live in GizmoSettings;
    //  * the selected entity's WORLD matrix goes in, a changed world matrix comes out, and SceneEditing::ApplyWorldMatrix
    //    turns it into the local position / rotation / scale (parent-aware, tested);
    //  * IsOver() / IsUsing() keep a click on a handle from also picking whatever is behind it.
    // ==================================================================
    enum class GizmoOperation : uint8_t { None, Translate, Rotate, Scale };
    enum class GizmoSpace     : uint8_t { Local, World };

    struct GizmoSettings
    {
        GizmoOperation operation = GizmoOperation::Translate;
        GizmoSpace     space     = GizmoSpace::World;
        bool  snap = false;
        float translateSnap = 0.5f;           // world units
        float rotateSnapDegrees = 15.0f;
        float scaleSnap = 0.1f;

        // the step of the current tool (0 = no snapping)
        float SnapStep() const
        {
            if (!snap) return 0.0f;
            switch (operation)
            {
            case GizmoOperation::Translate: return translateSnap;
            case GizmoOperation::Rotate:    return rotateSnapDegrees;
            case GizmoOperation::Scale:     return scaleSnap;
            default:                        return 0.0f;
            }
        }
    };

    // value rounded to the nearest multiple of step (step <= 0: unchanged)
    inline float SnapToStep(float value, float step)
    {
        return step > 0.0f ? std::round(value / step) * step : value;
    }

    // Where the scene is on screen and how it is seen. Matrices are DirectXMath's: row-vector, left-handed, row-major.
    // (A backend that expects another convention converts here, in one place.)
    struct GizmoView
    {
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
        float x = 0, y = 0, width = 0, height = 0;     // the viewport rectangle in screen pixels
    };

    class IGizmo
    {
    public:
        virtual ~IGizmo() = default;
        virtual const char* Name() const = 0;
        virtual bool IsAvailable() const = 0;          // false = draws nothing, changes nothing

        // Draws the gizmo for the entity whose world matrix is 'world' and lets the user drag it. Returns true when the
        // matrix was changed this frame (then 'world' holds the new one).
        virtual bool Manipulate(const GizmoView& view, const GizmoSettings& settings, DirectX::XMFLOAT4X4& world) = 0;

        virtual bool IsUsing() const = 0;              // a drag is in progress
        virtual bool IsOver() const = 0;               // the cursor is over a handle
    };

    // The default: no widget.
    class NullGizmo final : public IGizmo
    {
    public:
        const char* Name() const override { return "none"; }
        bool IsAvailable() const override { return false; }
        bool Manipulate(const GizmoView&, const GizmoSettings&, DirectX::XMFLOAT4X4&) override { return false; }
        bool IsUsing() const override { return false; }
        bool IsOver() const override { return false; }
    };
}
