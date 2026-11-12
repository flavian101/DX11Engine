#pragma once

namespace DXEngine
{
    // ==================================================================
    // RendererPanel: the "Renderer" debug window. Frame time, the renderer's statistics, the debug toggles and the
    // environment / shadow settings, all read from and written to the Renderer itself (so keys that change the same
    // things stay in sync). Needs the ImGui layer to be active; call it from a Layer's OnUIRender.
    // ==================================================================
    class RendererPanel
    {
    public:
        // 'open' (optional) is the window's close button state.
        static void OnImGui(bool* open = nullptr);
    };
}
