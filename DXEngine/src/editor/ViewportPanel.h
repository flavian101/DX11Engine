#pragma once
#include "EditorContext.h"
#include "ViewportMath.h"

namespace DXEngine
{
    // ==================================================================
    // The "Viewport" window: the scene, drawn into a texture that fills the panel.
    //
    // Every frame it asks the renderer for a scene texture the size of the panel (applied at the NEXT BeginScene, so the image
    // is one frame behind a resize), keeps the camera's aspect ratio equal to the panel's, and shows the texture.
    // On top of the image: a toolbar, the outline of the selected entity, and the gizmo (when a backend is installed).
    //
    //   left click        pick the entity under the cursor (not while a gizmo handle is under it)
    //   middle button     fly the camera (handled by Application: cursor capture); WASD move
    //   Q W E R           select / move / rotate / scale tool          (while the viewport has focus, not flying)
    //   F                 bring the selection into view
    //   Del, Ctrl+D       delete, duplicate the selection
    // The tool buttons live in the toolbar (EditorChrome); the viewport itself stays clean: the picture, the outline, the gizmo
    // and, while flying, the camera speed.
    //
    // It writes ctx.viewportHovered / viewportFocused for the host and the other panels.
    // ==================================================================
    class ViewportPanel
    {
    public:
        void OnImGui(EditorContext& ctx, bool* open);

        // the size of the scene texture last asked for (what picking maps the mouse to)
        ViewportMath::Size TargetSize() const { return m_Size; }

        // Moves the camera back along its view direction until the selection is in view (the F key and Edit > Frame selected).
        void FrameSelection(EditorContext& ctx);

        // where the picture was drawn last frame, in screen pixels (tests aim the mouse with it)
        struct Rect { float x = 0, y = 0, width = 0, height = 0; };
        Rect ImageRect() const { return m_ImageRect; }

    private:
        void DrawSelectionOutline(EditorContext& ctx, float imageX, float imageY, float imageW, float imageH);
        void HandleShortcuts(EditorContext& ctx);
        void DrawFlyHud(EditorContext& ctx, float imageX, float imageY, float imageW);

        ViewportMath::Size m_Size;
        Rect m_ImageRect;
    };
}
