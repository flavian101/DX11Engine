#pragma once

namespace DXEngine
{
    // ==================================================================
    // When does the UI keep an input event from the rest of the application? Pure, so the rules are tested.
    //
    // 'wantMouse' is ImGui's io.WantCaptureMouse, and it is TRUE for as long as a mouse button that went down over a panel
    // is held, wherever the pointer goes, even with ImGui's NoMouse flag set. So while the camera owns the mouse (middle
    // button held over the viewport: cursor captured, re-centred every move) it is true the whole time and, believed,
    // would swallow every mouse-move event the camera needs. 'mouseEnabled' = false (the camera owns the mouse) wins over it.
    // ==================================================================
    struct UiInputState
    {
        bool active = false;              // the UI backend exists
        bool blockMouseEvents = true;     // the host did not hand the mouse to the scene (e.g. the cursor is over the viewport)
        bool mouseEnabled = true;         // false while something else (the camera) owns the mouse
        bool wantMouse = false;           // ImGui: io.WantCaptureMouse
        bool wantKeyboard = false;        // ImGui: io.WantCaptureKeyboard
    };

    inline bool ShouldBlockMouseEvent(const UiInputState& s)
    {
        return s.active && s.blockMouseEvents && s.mouseEnabled && s.wantMouse;
    }

    inline bool ShouldBlockKeyboardEvent(const UiInputState& s)
    {
        return s.active && s.wantKeyboard;
    }
}
