#pragma once
#include "imgui.h"

namespace DXEngine
{
    // ==================================================================
    // EditorTheme: how the editor looks, in one place. A dark, flat theme with one accent colour; roomy controls (taller
    // frames, 8 px spacing) because a property you can hit at the first try beats one that fits one more row.
    //
    // Apply(scale) rebuilds the whole style from scratch (so applying it again never compounds) and scales sizes and fonts:
    // the "UI scale" of the View menu, for high-DPI screens.
    // ==================================================================
    namespace EditorTheme
    {
        struct Palette
        {
            ImVec4 accent       { 0.28f, 0.56f, 0.96f, 1.00f };
            ImVec4 accentSoft   { 0.20f, 0.33f, 0.56f, 1.00f };
            ImVec4 text         { 0.90f, 0.91f, 0.93f, 1.00f };
            ImVec4 textDim      { 0.52f, 0.54f, 0.59f, 1.00f };
            ImVec4 warning      { 1.00f, 0.74f, 0.25f, 1.00f };
            ImVec4 danger       { 0.95f, 0.35f, 0.32f, 1.00f };
            // axes: the colours every 3D tool agrees on
            ImVec4 axisX        { 0.88f, 0.30f, 0.30f, 1.00f };
            ImVec4 axisY        { 0.38f, 0.78f, 0.34f, 1.00f };
            ImVec4 axisZ        { 0.30f, 0.50f, 0.95f, 1.00f };
            // entity kinds (hierarchy icons)
            ImVec4 kindMesh     { 0.55f, 0.74f, 0.98f, 1.00f };
            ImVec4 kindLight    { 1.00f, 0.84f, 0.38f, 1.00f };
            ImVec4 kindSky      { 0.70f, 0.58f, 0.95f, 1.00f };
            ImVec4 kindEmpty    { 0.62f, 0.64f, 0.69f, 1.00f };
        };

        const Palette& Colors();

        // Rebuilds the style. scale = 1 is the default size; clamped to [0.75, 2.5].
        void Apply(float scale = 1.0f);

        // the scale last applied
        float Scale();

        // the scales offered by the View menu
        constexpr float kScales[] = { 0.85f, 1.0f, 1.15f, 1.3f, 1.5f, 2.0f };
    }
}
