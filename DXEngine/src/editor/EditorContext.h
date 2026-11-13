#pragma once
#include "EditorSelection.h"
#include "GizmoTypes.h"
#include "camera/CameraController.h"
#include "scene/Scene.h"
#include <memory>

namespace DXEngine
{
    // ==================================================================
    // EditorContext: what the panels share. One instance, owned by Editor; panels get it by reference every frame.
    // It holds no GPU resources: the scene and the camera belong to the host (the game), the editor only edits them.
    // ==================================================================
    struct EditorContext
    {
        Scene* scene = nullptr;
        std::shared_ptr<CameraController> camera;

        EditorSelection selection;
        GizmoSettings   gizmoSettings;
        std::unique_ptr<IGizmo> gizmo = std::make_unique<NullGizmo>();

        // Something selected an entity from outside the hierarchy (a click in the viewport): the hierarchy opens the
        // chain above it and scrolls to it, then clears this.
        bool revealSelection = false;

        // written by the viewport panel every frame, read by the host and the other panels
        bool viewportHovered = false;
        bool viewportFocused = false;
        bool flying = false;                 // the camera owns the mouse (middle button held, cursor captured)
    };

    // The "create" items (empty entity, lights) as menu entries; the new entity goes under 'parent' (invalid = a root) and
    // becomes the selection. Shared by the main menu, the hierarchy's "+" button and its context menu.
    void DrawCreateMenu(EditorContext& ctx, Entity parent);
}
