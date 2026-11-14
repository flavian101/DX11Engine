#include "dxpch.h"
#include "EditorTheme.h"
#include <algorithm>

namespace DXEngine
{
    namespace EditorTheme
    {
        namespace { float g_scale = 1.0f; }

        const Palette& Colors()
        {
            static const Palette palette;
            return palette;
        }

        float Scale() { return g_scale; }

        void Apply(float scale)
        {
            g_scale = std::clamp(scale, 0.75f, 2.5f);
            const Palette& p = Colors();

            ImGuiStyle style;                                  // fresh defaults every time: ScaleAllSizes would compound
            ImGui::StyleColorsDark(&style);

            style.WindowPadding     = ImVec2(10, 10);
            style.FramePadding      = ImVec2(8, 5);
            style.CellPadding       = ImVec2(6, 4);
            style.ItemSpacing       = ImVec2(8, 6);
            style.ItemInnerSpacing  = ImVec2(6, 4);
            style.IndentSpacing     = 16.0f;
            style.ScrollbarSize     = 12.0f;
            style.GrabMinSize       = 10.0f;
            style.DockingSeparatorSize = 2.0f;

            style.WindowBorderSize  = 0.0f;
            style.ChildBorderSize   = 0.0f;
            style.PopupBorderSize   = 1.0f;
            style.FrameBorderSize   = 0.0f;
            style.TabBorderSize     = 0.0f;

            style.WindowRounding    = 0.0f;                    // docked windows meet at the edges
            style.ChildRounding     = 4.0f;
            style.FrameRounding     = 4.0f;
            style.PopupRounding     = 6.0f;
            style.ScrollbarRounding = 6.0f;
            style.GrabRounding      = 4.0f;
            style.TabRounding       = 4.0f;

            style.WindowMenuButtonPosition = ImGuiDir_None;    // no collapse arrow in the tab / title
            style.SeparatorTextBorderSize  = 1.0f;

            const ImVec4 clear(0, 0, 0, 0);
            ImVec4* c = style.Colors;
            c[ImGuiCol_Text]                 = p.text;
            c[ImGuiCol_TextDisabled]         = p.textDim;
            c[ImGuiCol_WindowBg]             = ImVec4(0.105f, 0.110f, 0.125f, 1.0f);
            c[ImGuiCol_ChildBg]              = clear;
            c[ImGuiCol_PopupBg]              = ImVec4(0.130f, 0.138f, 0.160f, 0.98f);
            c[ImGuiCol_Border]               = ImVec4(0.20f, 0.21f, 0.24f, 1.0f);
            c[ImGuiCol_BorderShadow]         = clear;
            c[ImGuiCol_FrameBg]              = ImVec4(0.155f, 0.165f, 0.195f, 1.0f);
            c[ImGuiCol_FrameBgHovered]       = ImVec4(0.200f, 0.215f, 0.255f, 1.0f);
            c[ImGuiCol_FrameBgActive]        = ImVec4(0.235f, 0.265f, 0.330f, 1.0f);
            c[ImGuiCol_TitleBg]              = ImVec4(0.075f, 0.080f, 0.095f, 1.0f);
            c[ImGuiCol_TitleBgActive]        = ImVec4(0.075f, 0.080f, 0.095f, 1.0f);
            c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.075f, 0.080f, 0.095f, 1.0f);
            c[ImGuiCol_MenuBarBg]            = ImVec4(0.075f, 0.080f, 0.095f, 1.0f);
            c[ImGuiCol_ScrollbarBg]          = clear;
            c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.30f, 0.35f, 1.0f);
            c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.39f, 0.45f, 1.0f);
            c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.44f, 0.48f, 0.55f, 1.0f);
            c[ImGuiCol_CheckMark]            = p.accent;
            c[ImGuiCol_SliderGrab]           = p.accent;
            c[ImGuiCol_SliderGrabActive]     = ImVec4(0.45f, 0.68f, 1.00f, 1.0f);
            c[ImGuiCol_Button]               = ImVec4(0.200f, 0.220f, 0.270f, 1.0f);
            c[ImGuiCol_ButtonHovered]        = ImVec4(0.260f, 0.290f, 0.360f, 1.0f);
            c[ImGuiCol_ButtonActive]         = ImVec4(0.220f, 0.380f, 0.660f, 1.0f);
            c[ImGuiCol_Header]               = ImVec4(0.180f, 0.240f, 0.360f, 1.0f);
            c[ImGuiCol_HeaderHovered]        = ImVec4(0.230f, 0.310f, 0.470f, 1.0f);
            c[ImGuiCol_HeaderActive]         = ImVec4(0.260f, 0.370f, 0.590f, 1.0f);
            c[ImGuiCol_Separator]            = ImVec4(0.20f, 0.21f, 0.24f, 1.0f);
            c[ImGuiCol_SeparatorHovered]     = p.accent;
            c[ImGuiCol_SeparatorActive]      = p.accent;
            c[ImGuiCol_ResizeGrip]           = ImVec4(0.28f, 0.30f, 0.35f, 0.5f);
            c[ImGuiCol_ResizeGripHovered]    = p.accent;
            c[ImGuiCol_ResizeGripActive]     = p.accent;
            c[ImGuiCol_Tab]                  = ImVec4(0.095f, 0.100f, 0.118f, 1.0f);
            c[ImGuiCol_TabHovered]           = ImVec4(0.260f, 0.420f, 0.700f, 1.0f);
            c[ImGuiCol_TabSelected]          = ImVec4(0.150f, 0.175f, 0.235f, 1.0f);
            c[ImGuiCol_TabSelectedOverline]  = p.accent;
            c[ImGuiCol_TabDimmed]            = ImVec4(0.085f, 0.090f, 0.105f, 1.0f);
            c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.125f, 0.140f, 0.180f, 1.0f);
            c[ImGuiCol_DockingPreview]       = ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.40f);
            c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.070f, 0.075f, 0.088f, 1.0f);
            c[ImGuiCol_TableHeaderBg]        = ImVec4(0.130f, 0.140f, 0.165f, 1.0f);
            c[ImGuiCol_TableBorderStrong]    = ImVec4(0.20f, 0.21f, 0.24f, 1.0f);
            c[ImGuiCol_TableBorderLight]     = ImVec4(0.16f, 0.17f, 0.20f, 1.0f);
            c[ImGuiCol_TableRowBg]           = clear;
            c[ImGuiCol_TableRowBgAlt]        = ImVec4(1, 1, 1, 0.02f);
            c[ImGuiCol_TextSelectedBg]       = ImVec4(p.accent.x, p.accent.y, p.accent.z, 0.35f);
            c[ImGuiCol_DragDropTarget]       = p.accent;
            c[ImGuiCol_NavCursor]            = p.accent;

            style.ScaleAllSizes(g_scale);
            style.FontScaleMain = g_scale;                     // crisp at any size: the font is re-rasterised, not stretched
            ImGui::GetStyle() = style;
        }
    }
}
