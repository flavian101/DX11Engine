#pragma once
#include "EditorContext.h"
#include "ViewportMath.h"
#include "imgui.h"

namespace DXEngine
{
    // ==================================================================
    // The "Viewport" window: the scene, drawn into a texture that fills the panel.
    //
    // Every frame it asks the renderer for a scene texture the size of the panel (applied at the NEXT BeginScene, so the image
    // is one frame behind a resize), keeps the camera's aspect ratio equal to the panel's, and shows the texture.
    // On top of the image: a toolbar, the outline of the selected entity, and the gizmo (when a backend is installed).
    //
    //   left click        pick the entity under the cursor (not while a gizmo handle is under it); Ctrl / Shift: add it to the
    //                     selection (or take it out, with Ctrl); on empty space it clears the selection
    //   left drag         a box over empty space (or over anything: no gizmo handle under the mouse): selects every entity whose
    //                     center is inside it; Ctrl / Shift: adds them to the selection
    //   gizmo             moves the whole selection as one rigid group around the PRIMARY entity (the one selected last)
    //   middle button     fly the camera (handled by Application: cursor capture); WASD move
    //   Q W E R           select / move / rotate / scale tool          (while the viewport has focus, not flying)
    //   F                 bring the selection into view
    //   Del, Ctrl+D       delete, duplicate the selection (a selected parent takes its selected children along: nothing twice)
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
        void FinishClickOrBox(EditorContext& ctx, const ImVec2& mouse, float imageX, float imageY, float imageW, float imageH);

        ViewportMath::Size m_Size;
        Rect m_ImageRect;

        // a left press in the picture is a click (pick) or the start of a box; which one is known when the button is released
        bool   m_Pressing = false;
        bool   m_Boxing = false;                     // the mouse moved far enough from the press: it is a box
        ImVec2 m_PressPos;
    };
}
