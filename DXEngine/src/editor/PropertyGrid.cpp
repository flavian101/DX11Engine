#include "dxpch.h"
#include "PropertyGrid.h"
#include "EditorTheme.h"

#include "imgui.h"
#include "imgui_internal.h"      // PushMultiItemsWidths
#include <cstdarg>

namespace DXEngine
{
    namespace PropertyGrid
    {
        bool Begin(const char* id)
        {
            ImGui::PushID(id);
            if (!ImGui::BeginTable("##grid", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_PadOuterX))
            {
                ImGui::PopID();
                return false;
            }
            ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthStretch, 0.40f);
            ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch, 0.60f);
            return true;
        }

        void End()
        {
            ImGui::EndTable();
            ImGui::PopID();
        }

        void Label(const char* label, const char* tooltip)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            if (tooltip && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tooltip);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
        }

        namespace
        {
            // one axis: a coloured letter button (click = reset) followed by the drag field
            bool Axis(const char* letter, const ImVec4& color, float& value, float resetValue, float speed, const char* format, float letterWidth)
            {
                bool changed = false;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.75f, color.y * 0.75f, color.z * 0.75f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, ImGui::GetStyle().FramePadding.y));
                if (ImGui::Button(letter, ImVec2(letterWidth, 0.0f)))
                {
                    value = resetValue;
                    changed = true;
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Reset %s", letter);

                ImGui::SameLine(0.0f, 0.0f);
                char id[8];
                snprintf(id, sizeof(id), "##%s", letter);
                changed |= ImGui::DragFloat(id, &value, speed, 0.0f, 0.0f, format);
                return changed;
            }
        }

        bool Vec3(const char* label, DirectX::XMFLOAT3& v, float speed, DirectX::XMFLOAT3 reset, const char* format)
        {
            Label(label);
            ImGui::PushID(label);
            const EditorTheme::Palette& p = EditorTheme::Colors();

            const float letterWidth = ImGui::GetFrameHeight() * 0.8f;
            ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
            bool changed = false;
            changed |= Axis("X", p.axisX, v.x, reset.x, speed, format, letterWidth);
            ImGui::PopItemWidth();
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            changed |= Axis("Y", p.axisY, v.y, reset.y, speed, format, letterWidth);
            ImGui::PopItemWidth();
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            changed |= Axis("Z", p.axisZ, v.z, reset.z, speed, format, letterWidth);
            ImGui::PopItemWidth();

            ImGui::PopID();
            return changed;
        }

        bool Float(const char* label, float& value, float speed, float min, float max, const char* format)
        {
            Label(label);
            ImGui::PushID(label);
            const bool changed = ImGui::DragFloat("##v", &value, speed, min, max, format);
            ImGui::PopID();
            return changed;
        }

        bool Slider(const char* label, float& value, float min, float max, const char* format)
        {
            Label(label);
            ImGui::PushID(label);
            const bool changed = ImGui::SliderFloat("##v", &value, min, max, format);
            ImGui::PopID();
            return changed;
        }

        bool Checkbox(const char* label, bool& value)
        {
            Label(label);
            ImGui::PushID(label);
            const bool changed = ImGui::Checkbox("##v", &value);
            ImGui::PopID();
            return changed;
        }

        bool Color3(const char* label, float* rgb, bool hdr)
        {
            Label(label);
            ImGui::PushID(label);
            const bool changed = ImGui::ColorEdit3("##v", rgb, hdr ? ImGuiColorEditFlags_HDR : ImGuiColorEditFlags_None);
            ImGui::PopID();
            return changed;
        }

        bool Color4(const char* label, float* rgba)
        {
            Label(label);
            ImGui::PushID(label);
            const bool changed = ImGui::ColorEdit4("##v", rgba, ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::PopID();
            return changed;
        }

        bool Combo(const char* label, int& index, const char* const* items, int count)
        {
            Label(label);
            ImGui::PushID(label);
            const bool changed = ImGui::Combo("##v", &index, items, count);
            ImGui::PopID();
            return changed;
        }

        void Text(const char* label, const char* format, ...)
        {
            Label(label);
            va_list args;
            va_start(args, format);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::Colors().textDim);
            ImGui::TextV(format, args);
            ImGui::PopStyleColor();
            va_end(args);
        }
    }
}
