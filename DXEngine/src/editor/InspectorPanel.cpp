#include "dxpch.h"
#include "InspectorPanel.h"
#include "EditorIcons.h"
#include "EditorTheme.h"
#include "PropertyGrid.h"
#include "SceneEditing.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"

#include "imgui.h"

namespace DXEngine
{
    namespace
    {
        const char* ShadingModelName(ShadingModel m)
        {
            switch (m)
            {
            case ShadingModel::Unlit:    return "Unlit";
            case ShadingModel::Lit:      return "Lit (PBR)";
            case ShadingModel::Emissive: return "Emissive";
            case ShadingModel::Sky:      return "Sky";
            default:                     return "?";
            }
        }

        // A collapsible section header. 'remove' (optional) adds a close button at the right: true when it was pressed.
        bool Section(const char* label, bool* remove, bool defaultOpen = true)
        {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth |
                                       ImGuiTreeNodeFlags_FramePadding;
            if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;
            const bool open = ImGui::TreeNodeEx(label, flags);
            if (remove)
            {
                const float h = ImGui::GetFrameHeight();
                ImGui::SameLine(ImGui::GetWindowWidth() - h - ImGui::GetStyle().WindowPadding.x - ImGui::GetStyle().ItemSpacing.x * 0.5f);
                if (EditorIcons::Button("##removecomponent", Icon::Close, false, "Remove component", h))
                    *remove = true;
            }
            return open;
        }

        void DrawMaterial(Material& m, int index)
        {
            MaterialProperties& p = m.GetProperties();       // read here, written only through the setters below

            // the title carries a colour swatch, so a long list can be told apart at a glance
            const std::string title = "     " + m.GetName() + "##mat" + std::to_string(index);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const bool open = ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
            const float h = ImGui::GetTextLineHeight();
            const float x0 = start.x + ImGui::GetTreeNodeToLabelSpacing();           // where the label begins: the swatch goes in the blank we left
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x0, start.y + 1.0f), ImVec2(x0 + h * 1.3f, start.y + 1.0f + h),
                                                      ImGui::ColorConvertFloat4ToU32(ImVec4(p.diffuseColor.x, p.diffuseColor.y, p.diffuseColor.z, 1.0f)), 3.0f);
            if (!open) return;

            if (PropertyGrid::Begin("material"))
            {
                PropertyGrid::Text("Shading", "%s", ShadingModelName(m.GetShadingModel()));

                DirectX::XMFLOAT4 diffuse = p.diffuseColor;
                if (PropertyGrid::Color4("Colour", &diffuse.x)) m.SetDiffuseColor(diffuse);

                if (m.GetShadingModel() == ShadingModel::Lit)
                {
                    float metallic = p.metallic, roughness = p.roughness;
                    if (PropertyGrid::Slider("Metallic", metallic, 0.0f, 1.0f))   m.SetMetallic(metallic);
                    if (PropertyGrid::Slider("Roughness", roughness, 0.0f, 1.0f)) m.SetRoughness(roughness);
                }

                DirectX::XMFLOAT4 emissive = p.emissiveColor;
                if (PropertyGrid::Color3("Emissive", &emissive.x, true)) m.SetEmissiveColor(emissive);
                float intensity = p.emissiveIntensity;
                if (PropertyGrid::Float("Emissive power", intensity, 0.05f, 0.0f, 100.0f)) m.SetEmissiveIntensity(intensity);

                float alpha = p.alpha;
                if (PropertyGrid::Slider("Alpha", alpha, 0.0f, 1.0f)) m.SetAlpha(alpha);

                DirectX::XMFLOAT3 tiling = { p.textureScale.x, p.textureScale.y, 0.0f };
                if (PropertyGrid::Float("Tiling U", tiling.x, 0.05f, 0.01f, 1000.0f) | PropertyGrid::Float("Tiling V", tiling.y, 0.05f, 0.01f, 1000.0f))
                    m.SetTextureScale({ tiling.x, tiling.y });

                PropertyGrid::Text("Blending", "%s", m.IsBlended() ? "blended" : "opaque");
                PropertyGrid::End();
            }
            ImGui::TextDisabled("Shared: this changes every object that uses it.");
            ImGui::TreePop();
        }
    }

    void InspectorPanel::OnImGui(EditorContext& ctx, bool* open)
    {
        if (open && !*open) return;
        if (!ImGui::Begin("Inspector", open))
        {
            ImGui::End();
            return;
        }

        Entity e = ctx.selection.Get();
        if (!e.IsValid())
        {
            const float w = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y * 0.30f);
            const char* line1 = "Nothing selected";
            const char* line2 = "Click an object in the viewport or the hierarchy.";
            ImGui::SetCursorPosX((w - ImGui::CalcTextSize(line1).x) * 0.5f);
            ImGui::TextUnformatted(line1);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::Colors().textDim);
            ImGui::PushTextWrapPos();
            ImGui::TextWrapped("%s", line2);
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::End();
            return;
        }

        // a new selection starts the Euler angles from scratch and the name field from the new name
        if (e != m_LastEntity)
        {
            m_LastEntity = e;
            m_Euler.Reset();
            m_EditingName = false;
        }

        DrawHeader(ctx, e);
        ImGui::Spacing();

        DrawTransform(e);
        if (e.Has<MeshRendererComponent>()) DrawMeshRenderer(e);
        if (e.Has<LightComponent>())        DrawLight(e);

        const EditorTheme::Palette& pal = EditorTheme::Colors();
        ImGui::PushStyleColor(ImGuiCol_Text, pal.textDim);
        if (e.Has<SkyboxComponent>())            ImGui::TextUnformatted("Skybox: not editable yet");
        if (e.Has<SkinnedMeshComponent>())       ImGui::TextUnformatted("Skinned mesh: not editable yet");
        if (e.Has<AnimationUpdateComponent>())   ImGui::TextUnformatted("Animation: not editable yet");
        ImGui::PopStyleColor();

        // removals requested by the section headers' close buttons: after everything was drawn
        if (m_RemoveLight)    { e.Remove<LightComponent>();        m_RemoveLight = false; }
        if (m_RemoveRenderer) { e.Remove<MeshRendererComponent>(); m_RemoveRenderer = false; }

        ImGui::End();
    }

    void InspectorPanel::DrawHeader(EditorContext& ctx, Entity e)
    {
        const EditorTheme::Palette& pal = EditorTheme::Colors();
        const float h = ImGui::GetFrameHeight();

        // kind icon
        Icon icon = Icon::Entity;
        ImVec4 color = pal.kindEmpty;
        if (const LightComponent* l = e.TryGet<LightComponent>())
        {
            icon = l->type == LightType::Directional ? Icon::Sun : l->type == LightType::Spot ? Icon::Spot : Icon::Light;
            color = pal.kindLight;
        }
        else if (e.Has<SkyboxComponent>())       { icon = Icon::Sky;  color = pal.kindSky; }
        else if (e.Has<MeshRendererComponent>()) { icon = Icon::Mesh; color = pal.kindMesh; }
        const ImVec2 p = ImGui::GetCursorScreenPos();
        EditorIcons::Draw(ImGui::GetWindowDrawList(), icon, ImVec2(p.x + h * 0.5f, p.y + h * 0.5f), h * 0.75f, ImGui::ColorConvertFloat4ToU32(color));
        ImGui::Dummy(ImVec2(h, h));
        ImGui::SameLine();

        // name (the buffer follows the component, except while the user is typing in it)
        NameComponent& name = e.Get<NameComponent>();
        if (!m_EditingName)
            snprintf(m_NameBuffer, sizeof(m_NameBuffer), "%s", name.name.c_str());
        const float addWidth = ImGui::CalcTextSize("Add component").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetNextItemWidth(-(addWidth + ImGui::GetStyle().ItemSpacing.x));
        if (ImGui::InputText("##name", m_NameBuffer, sizeof(m_NameBuffer)))
            name.name = m_NameBuffer;
        m_EditingName = ImGui::IsItemActive();

        // add component
        ImGui::SameLine();
        if (ImGui::Button("Add component"))
            ImGui::OpenPopup("##addcomponent");
        if (ImGui::BeginPopup("##addcomponent"))
        {
            if (ImGui::BeginMenu("Light", !e.Has<LightComponent>()))
            {
                static const struct { const char* name; LightType type; } kTypes[] = {
                    { "Point", LightType::Point }, { "Spot", LightType::Spot }, { "Directional", LightType::Directional } };
                for (const auto& t : kTypes)
                    if (ImGui::MenuItem(t.name))
                    {
                        LightComponent light;
                        light.type = t.type;
                        e.Add<LightComponent>(light);
                    }
                ImGui::EndMenu();
            }
            ImGui::TextDisabled("(meshes come with the asset system)");
            ImGui::EndPopup();
        }

        // a dim sub line: id and parent
        Entity parent = ctx.scene ? ctx.scene->GetParent(e) : Entity();
        ImGui::PushStyleColor(ImGuiCol_Text, pal.textDim);
        if (parent.IsValid())
            ImGui::Text("Entity %u   child of %s", static_cast<unsigned>(entt::to_integral(e.Id())), parent.Get<NameComponent>().name.c_str());
        else
            ImGui::Text("Entity %u", static_cast<unsigned>(entt::to_integral(e.Id())));
        ImGui::PopStyleColor();
    }

    void InspectorPanel::DrawTransform(Entity e)
    {
        if (!Section("Transform", nullptr))
            return;
        TransformComponent& t = e.Get<TransformComponent>();

        if (PropertyGrid::Begin("transform"))
        {
            PropertyGrid::Vec3("Position", t.position, 0.05f, { 0, 0, 0 });

            DirectX::XMFLOAT3 euler = m_Euler.Get(t.rotation);
            if (PropertyGrid::Vec3("Rotation", euler, 0.5f, { 0, 0, 0 }, "%.1f"))
                t.rotation = m_Euler.Set(euler);

            const DirectX::XMFLOAT3 before = t.scale;
            if (PropertyGrid::Vec3("Scale", t.scale, 0.01f, { 1, 1, 1 }) && m_UniformScale)
                t.scale = TransformEditing::ApplyUniformScaleEdit(before, t.scale);
            PropertyGrid::Checkbox("Uniform scale", m_UniformScale);

            const DirectX::XMFLOAT3 wp = t.WorldPosition();
            PropertyGrid::Text("World position", "%.2f  %.2f  %.2f", wp.x, wp.y, wp.z);
            PropertyGrid::End();
        }
        if (ImGui::SmallButton("Reset transform"))
        {
            t.position = { 0, 0, 0 };
            t.rotation = { 0, 0, 0, 1 };
            t.scale = { 1, 1, 1 };
            m_Euler.Reset();
        }
        ImGui::TreePop();
    }

    void InspectorPanel::DrawMeshRenderer(Entity e)
    {
        bool remove = false;
        const bool open = Section("Mesh renderer", &remove);
        if (remove) m_RemoveRenderer = true;
        if (!open) return;

        MeshRendererComponent& mr = e.Get<MeshRendererComponent>();
        if (PropertyGrid::Begin("meshrenderer"))
        {
            PropertyGrid::Checkbox("Visible", mr.visible);
            PropertyGrid::Checkbox("Casts shadows", mr.castsShadows);
            PropertyGrid::Checkbox("Receives shadows", mr.receivesShadows);
            if (mr.mesh)
            {
                const BoundingBox& b = mr.mesh->GetBoundingBox();
                PropertyGrid::Text("Vertices", "%zu", mr.mesh->GetVertexCount());
                PropertyGrid::Text("Indices", "%zu", mr.mesh->GetIndexCount());
                PropertyGrid::Text("Size", "%.2f x %.2f x %.2f", b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z);
            }
            else
                PropertyGrid::Text("Mesh", "none");
            PropertyGrid::End();
        }

        if (mr.materials.empty())
            ImGui::TextDisabled("Uses the mesh's own material.");
        else
            ImGui::SeparatorText("Materials");
        for (size_t i = 0; i < mr.materials.size(); ++i)
            if (mr.materials[i])
                DrawMaterial(*mr.materials[i], static_cast<int>(i));
        ImGui::TreePop();
    }

    void InspectorPanel::DrawLight(Entity e)
    {
        bool remove = false;
        const bool open = Section("Light", &remove);
        if (remove) m_RemoveLight = true;
        if (!open) return;

        LightComponent& l = e.Get<LightComponent>();
        if (PropertyGrid::Begin("light"))
        {
            static const char* kTypes[] = { "Point", "Spot", "Directional" };
            int type = l.type == LightType::Point ? 0 : l.type == LightType::Spot ? 1 : 2;
            if (PropertyGrid::Combo("Type", type, kTypes, 3))
                l.type = type == 0 ? LightType::Point : type == 1 ? LightType::Spot : LightType::Directional;

            PropertyGrid::Color3("Colour", &l.color.x);
            PropertyGrid::Float("Intensity", l.intensity, 0.05f, 0.0f, 10000.0f);
            if (l.type != LightType::Directional)
                PropertyGrid::Float("Range", l.range, 0.1f, 0.1f, 1000.0f, "%.1f m");
            if (l.type == LightType::Spot)
            {
                PropertyGrid::Slider("Outer cone", l.outerConeDegrees, 1.0f, 89.0f, "%.0f deg");
                PropertyGrid::Slider("Inner cone", l.innerConeDegrees, 0.0f, l.outerConeDegrees, "%.0f deg");
            }
            PropertyGrid::Checkbox("Casts shadows", l.castsShadows);
            PropertyGrid::End();
        }
        ImGui::TreePop();
    }
}
