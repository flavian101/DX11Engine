#include "dxpch.h"
#include "EditorIcons.h"
#include "EditorTheme.h"
#include <cmath>

namespace DXEngine
{
    namespace EditorIcons
    {
        namespace
        {
            constexpr float kPi = 3.14159265f;

            // unit coordinates (-0.5 .. 0.5) -> pixels
            struct Space
            {
                ImVec2 c; float s;
                ImVec2 operator()(float x, float y) const { return ImVec2(c.x + x * s, c.y + y * s); }
            };

            void Arrow(ImDrawList* l, const Space& sp, float x0, float y0, float x1, float y1, ImU32 col, float th)
            {
                l->AddLine(sp(x0, y0), sp(x1, y1), col, th);
                // head: a small triangle at the tip, pointing along the line
                const float dx = x1 - x0, dy = y1 - y0;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len < 1e-4f) return;
                const float ux = dx / len, uy = dy / len, h = 0.16f, w = 0.10f;
                l->AddTriangleFilled(sp(x1 + ux * 0.02f, y1 + uy * 0.02f),
                                     sp(x1 - ux * h - uy * w, y1 - uy * h + ux * w),
                                     sp(x1 - ux * h + uy * w, y1 - uy * h - ux * w), col);
            }

            void Rays(ImDrawList* l, const Space& sp, float cx, float cy, float r0, float r1, int count, ImU32 col, float th)
            {
                for (int i = 0; i < count; ++i)
                {
                    const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(count);
                    l->AddLine(sp(cx + std::cos(a) * r0, cy + std::sin(a) * r0), sp(cx + std::cos(a) * r1, cy + std::sin(a) * r1), col, th);
                }
            }

            void EyeShape(ImDrawList* l, const Space& sp, ImU32 col, float th)
            {
                ImVec2 pts[24];
                for (int i = 0; i < 12; ++i)                   // upper lid, left to right
                {
                    const float t = -1.0f + 2.0f * static_cast<float>(i) / 11.0f;
                    pts[i] = sp(t * 0.46f, -0.26f * (1.0f - t * t));
                }
                for (int i = 0; i < 12; ++i)                   // lower lid, right to left
                {
                    const float t = 1.0f - 2.0f * static_cast<float>(i) / 11.0f;
                    pts[12 + i] = sp(t * 0.46f, 0.26f * (1.0f - t * t));
                }
                l->AddPolyline(pts, 24, col, ImDrawFlags_Closed, th);
                l->AddCircleFilled(sp(0, 0), sp.s * 0.13f, col);
            }
        }

        void Draw(ImDrawList* l, Icon icon, ImVec2 center, float size, ImU32 col)
        {
            const Space sp{ center, size };
            const float th = std::max(1.0f, size * 0.085f);

            switch (icon)
            {
            case Icon::Entity:       // a dashed-corner square: "an empty thing"
            {
                const float a = 0.34f, k = 0.14f;
                for (int sx : { -1, 1 })
                    for (int sy : { -1, 1 })
                    {
                        l->AddLine(sp(sx * a, sy * a), sp(sx * (a - k), sy * a), col, th);
                        l->AddLine(sp(sx * a, sy * a), sp(sx * a, sy * (a - k)), col, th);
                    }
                l->AddCircleFilled(sp(0, 0), size * 0.07f, col);
                break;
            }
            case Icon::Mesh:         // a cube
            {
                const float r = 0.42f;
                ImVec2 hex[6];
                for (int i = 0; i < 6; ++i)
                {
                    const float a = kPi / 6.0f + 2.0f * kPi * static_cast<float>(i) / 6.0f;
                    hex[i] = sp(std::cos(a) * r, std::sin(a) * r);
                }
                l->AddPolyline(hex, 6, col, ImDrawFlags_Closed, th);
                l->AddLine(sp(0, 0), hex[1], col, th);
                l->AddLine(sp(0, 0), hex[3], col, th);
                l->AddLine(sp(0, 0), hex[5], col, th);
                break;
            }
            case Icon::Light:        // a bulb
                l->AddCircle(sp(0, -0.10f), size * 0.24f, col, 20, th);
                l->AddLine(sp(-0.12f, 0.24f), sp(0.12f, 0.24f), col, th);
                l->AddLine(sp(-0.09f, 0.36f), sp(0.09f, 0.36f), col, th);
                Rays(l, sp, 0.0f, -0.10f, 0.34f, 0.46f, 6, col, th);
                break;
            case Icon::Sun:          // a disc with rays
                l->AddCircleFilled(sp(0, 0), size * 0.17f, col);
                Rays(l, sp, 0.0f, 0.0f, 0.27f, 0.42f, 8, col, th);
                break;
            case Icon::Spot:         // a cone of light
                l->AddTriangle(sp(0, -0.40f), sp(-0.34f, 0.32f), sp(0.34f, 0.32f), col, th);
                l->AddLine(sp(-0.34f, 0.32f), sp(0.34f, 0.32f), col, th);
                l->AddCircleFilled(sp(0, -0.34f), size * 0.07f, col);
                break;
            case Icon::Sky:          // a cloud
                l->AddCircleFilled(sp(-0.16f, 0.06f), size * 0.18f, col);
                l->AddCircleFilled(sp(0.06f, -0.06f), size * 0.23f, col);
                l->AddCircleFilled(sp(0.26f, 0.08f), size * 0.15f, col);
                l->AddRectFilled(sp(-0.16f, 0.08f), sp(0.26f, 0.23f), col);
                break;

            case Icon::Select:       // the pointer
            {
                const ImVec2 poly[7] = { sp(-0.28f, -0.44f), sp(0.34f, 0.10f), sp(0.04f, 0.14f), sp(0.20f, 0.46f),
                                         sp(0.06f, 0.52f), sp(-0.10f, 0.20f), sp(-0.28f, 0.36f) };
                l->AddConvexPolyFilled(poly, 7, col);
                break;
            }
            case Icon::Move:         // four arrows
                Arrow(l, sp, 0, 0, 0, -0.44f, col, th);
                Arrow(l, sp, 0, 0, 0,  0.44f, col, th);
                Arrow(l, sp, 0, 0, -0.44f, 0, col, th);
                Arrow(l, sp, 0, 0,  0.44f, 0, col, th);
                break;
            case Icon::Rotate:       // an arc with a head
            {
                const int n = 18;
                ImVec2 arc[n];
                for (int i = 0; i < n; ++i)
                {
                    const float a = -0.35f * kPi + 1.6f * kPi * static_cast<float>(i) / static_cast<float>(n - 1);
                    arc[i] = sp(std::cos(a) * 0.36f, std::sin(a) * 0.36f);
                }
                l->AddPolyline(arc, n, col, ImDrawFlags_None, th);
                const ImVec2 tip = arc[n - 1];
                l->AddTriangleFilled(ImVec2(tip.x + size * 0.14f, tip.y - size * 0.02f),
                                     ImVec2(tip.x - size * 0.16f, tip.y - size * 0.06f),
                                     ImVec2(tip.x - size * 0.02f, tip.y + size * 0.16f), col);
                break;
            }
            case Icon::Scale:        // a box with an arrow to its corner
                l->AddRect(sp(-0.40f, -0.10f), sp(0.10f, 0.40f), col, 0.0f, 0, th);
                Arrow(l, sp, -0.05f, 0.05f, 0.42f, -0.42f, col, th);
                break;

            case Icon::Play:
                l->AddTriangleFilled(sp(-0.24f, -0.34f), sp(-0.24f, 0.34f), sp(0.36f, 0.0f), col);
                break;
            case Icon::Stop:
                l->AddRectFilled(sp(-0.28f, -0.28f), sp(0.28f, 0.28f), col, size * 0.05f);
                break;

            case Icon::Eye:    EyeShape(l, sp, col, th); break;
            case Icon::EyeOff: EyeShape(l, sp, col, th); l->AddLine(sp(-0.40f, 0.36f), sp(0.40f, -0.36f), col, th * 1.3f); break;
            case Icon::Search:
                l->AddCircle(sp(-0.08f, -0.08f), size * 0.26f, col, 18, th);
                l->AddLine(sp(0.12f, 0.12f), sp(0.40f, 0.40f), col, th * 1.3f);
                break;
            case Icon::Close:
                l->AddLine(sp(-0.26f, -0.26f), sp(0.26f, 0.26f), col, th * 1.2f);
                l->AddLine(sp(-0.26f, 0.26f), sp(0.26f, -0.26f), col, th * 1.2f);
                break;
            case Icon::Plus:
                l->AddLine(sp(-0.30f, 0), sp(0.30f, 0), col, th * 1.2f);
                l->AddLine(sp(0, -0.30f), sp(0, 0.30f), col, th * 1.2f);
                break;
            case Icon::Wireframe:    // a cube drawn as lines only
            {
                l->AddRect(sp(-0.38f, -0.20f), sp(0.14f, 0.32f), col, 0.0f, 0, th);
                l->AddRect(sp(-0.14f, -0.38f), sp(0.38f, 0.14f), col, 0.0f, 0, th);
                for (int sx : { 0, 1 }) for (int sy : { 0, 1 })
                    l->AddLine(sp(-0.38f + sx * 0.52f, -0.20f + sy * 0.52f), sp(-0.14f + sx * 0.52f, -0.38f + sy * 0.52f), col, th);
                break;
            }
            case Icon::Shadow:       // a disc and its shadow
                l->AddCircleFilled(sp(-0.10f, -0.14f), size * 0.20f, col);
                l->AddEllipseFilled(sp(0.06f, 0.30f), ImVec2(size * 0.34f, size * 0.10f), (col & 0x00FFFFFF) | 0x66000000);
                break;
            }
        }

        bool Button(const char* id, Icon icon, bool active, const char* tooltip, float size)
        {
            const float side = size > 0.0f ? size : ImGui::GetFrameHeight();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(side, side));
            const bool hovered = ImGui::IsItemHovered();

            const EditorTheme::Palette& pal = EditorTheme::Colors();
            ImDrawList* list = ImGui::GetWindowDrawList();
            if (active || hovered)
            {
                const ImVec4 bg = active ? ImVec4(pal.accent.x, pal.accent.y, pal.accent.z, 0.85f)
                                         : ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
                list->AddRectFilled(p, ImVec2(p.x + side, p.y + side), ImGui::ColorConvertFloat4ToU32(bg), ImGui::GetStyle().FrameRounding);
            }
            const ImVec4 fg = active ? ImVec4(1, 1, 1, 1) : (hovered ? pal.text : ImVec4(pal.text.x, pal.text.y, pal.text.z, 0.78f));
            Draw(list, icon, ImVec2(p.x + side * 0.5f, p.y + side * 0.5f), side * 0.62f, ImGui::ColorConvertFloat4ToU32(fg));

            if (tooltip && hovered)
                ImGui::SetTooltip("%s", tooltip);
            return pressed;
        }
    }
}
