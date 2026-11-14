#pragma once
#include "EditorContext.h"

namespace DXEngine
{
    // ==================================================================
    // The bars around the dock space: a toolbar under the menu and a status bar at the bottom.
    // Both are viewport side bars, so the dock space between them shrinks to fit (create them BEFORE the dock space).
    // ==================================================================
    namespace EditorChrome
    {
        struct ToolbarResult
        {
            bool togglePlay = false;       // the Play button was pressed
        };

        // Tools (select / move / rotate / scale), space (world / local), snapping, display switches, play.
        ToolbarResult DrawToolbar(EditorContext& ctx);

        // What is selected, a hint for what the mouse is over, and the frame / scene numbers.
        void DrawStatusBar(EditorContext& ctx, const char* hint);

        // The text of the status bar's right side: "144 FPS   12 entities   37 draws" (pure, tested)
        void FormatStats(char* out, size_t size, float fps, size_t entities, unsigned drawCalls, unsigned triangles);
    }
}
