#include "dxpch.h"
#include "InspectorPanel.h"
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

        void DrawMaterial(Material& m, int index)
        {
            const std::string title = m.GetName() + "##mat" + std::to_string(index);
            if (!ImGui::TreeNode(title.c_str()))
                return;

            ImGui::TextDisabled("%s - shared: changes every object using it", ShadingModelName(m.GetShadingModel()));

            MaterialProperties& p = m.GetProperties();       // read here, written only through the setters below

            DirectX::XMFLOAT4 diffuse = p.diffuseColor;
            if (ImGui::ColorEdit4("Colour", &diffuse.x)) m.SetDiffuseColor(diffuse);

            if (m.GetShadingModel() == ShadingModel::Lit)
            {
                float metallic = p.metallic, roughness = p.roughness;
                if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f))   m.SetMetallic(metallic);
                if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) m.SetRoughness(roughness);
            }

            DirectX::XMFLOAT4 emissive = p.emissiveColor;
            if (ImGui::ColorEdit3("Emissive", &emissive.x, ImGuiColorEditFlags_HDR)) m.SetEmissiveColor(emissive);
            float intensity = p.emissiveIntensity;
            if (ImGui::DragFloat("Emissive intensity", &intensity, 0.05f, 0.0f, 100.0f)) m.SetEmissiveIntensity(intensity);

            float alpha = p.alpha;
            if (ImGui::SliderFloat("Alpha", &alpha, 0.0f, 1.0f)) m.SetAlpha(alpha);

            DirectX::XMFLOAT2 tiling = p.textureScale;
            if (ImGui::DragFloat2("Texture tiling", &tiling.x, 0.01f, 0.01f, 100.0f)) m.SetTextureScale(tiling);

            ImGui::TextDisabled("Blend: %s   Render queue: %u", m.IsBlended() ? "blended" : "opaque", static_cast<unsigned>(m.GetRenderQueue()));
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
            ImGui::TextDisabled("Nothing selected");
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

        // ---- name ----   (the text buffer follows the component, except while the user is typing in it)
        NameComponent& name = e.Get<NameComponent>();
        if (!m_EditingName)
            snprintf(m_NameBuffer, sizeof(m_NameBuffer), "%s", name.name.c_str());
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##name", m_NameBuffer, sizeof(m_NameBuffer)))
            name.name = m_NameBuffer;
        m_EditingName = ImGui::IsItemActive();

        Entity parent = ctx.scene ? ctx.scene->GetParent(e) : Entity();
        if (parent.IsValid())
            ImGui::TextDisabled("Child of %s", parent.Get<NameComponent>().name.c_str());
        ImGui::Separator();

        DrawTransform(e);
        if (e.Has<MeshRendererComponent>()) DrawMeshRenderer(e);
        if (e.Has<LightComponent>())        DrawLight(e);

        if (e.Has<SkyboxComponent>())            ImGui::TextDisabled("Skybox (not editable yet)");
        if (e.Has<SkinnedMeshComponent>())       ImGui::TextDisabled("Skinned mesh (not editable yet)");
        if (e.Has<AnimationUpdateComponent>())   ImGui::TextDisabled("Animation (not editable yet)");

        // ---- add ----
        ImGui::Separator();
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
            ImGui::EndPopup();
        }

        // removals requested by the component headers' close buttons: after everything was drawn
        if (m_RemoveLight)    { e.Remove<LightComponent>();        m_RemoveLight = false; }
        if (m_RemoveRenderer) { e.Remove<MeshRendererComponent>(); m_RemoveRenderer = false; }

        ImGui::End();
    }

    void InspectorPanel::DrawTransform(Entity e)
    {
        if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            return;
        TransformComponent& t = e.Get<TransformComponent>();

        ImGui::DragFloat3("Position", &t.position.x, 0.05f);

        DirectX::XMFLOAT3 euler = m_Euler.Get(t.rotation);
        if (ImGui::DragFloat3("Rotation", &euler.x, 0.5f, 0.0f, 0.0f, "%.1f"))
            t.rotation = m_Euler.Set(euler);

        ImGui::DragFloat3("Scale", &t.scale.x, 0.01f);

        const DirectX::XMFLOAT3 wp = t.WorldPosition();
        ImGui::TextDisabled("World position  %.2f  %.2f  %.2f", wp.x, wp.y, wp.z);
        if (ImGui::SmallButton("Reset transform"))
        {
            t.position = { 0, 0, 0 };
            t.rotation = { 0, 0, 0, 1 };
            t.scale = { 1, 1, 1 };
            m_Euler.Reset();
        }
    }

    void InspectorPanel::DrawMeshRenderer(Entity e)
    {
        bool keep = true;
        const bool open = ImGui::CollapsingHeader("Mesh renderer", &keep, ImGuiTreeNodeFlags_DefaultOpen);
        if (!keep) m_RemoveRenderer = true;
        if (!open) return;

        MeshRendererComponent& mr = e.Get<MeshRendererComponent>();
        ImGui::Checkbox("Visible", &mr.visible);
        ImGui::Checkbox("Casts shadows", &mr.castsShadows);
        ImGui::SameLine();
        ImGui::Checkbox("Receives shadows", &mr.receivesShadows);

        if (mr.mesh)
        {
            const BoundingBox& b = mr.mesh->GetBoundingBox();
            ImGui::TextDisabled("%zu vertices, %zu indices", mr.mesh->GetVertexCount(), mr.mesh->GetIndexCount());
            ImGui::TextDisabled("Size  %.2f x %.2f x %.2f", b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z);
        }
        else
            ImGui::TextDisabled("No mesh");

        if (mr.materials.empty())
            ImGui::TextDisabled("Uses the mesh's own material");
        for (size_t i = 0; i < mr.materials.size(); ++i)
            if (mr.materials[i])
                DrawMaterial(*mr.materials[i], static_cast<int>(i));
    }

    void InspectorPanel::DrawLight(Entity e)
    {
        bool keep = true;
        const bool open = ImGui::CollapsingHeader("Light", &keep, ImGuiTreeNodeFlags_DefaultOpen);
        if (!keep) m_RemoveLight = true;
        if (!open) return;

        LightComponent& l = e.Get<LightComponent>();

        static const char* kTypes[] = { "Point", "Spot", "Directional" };
        int type = l.type == LightType::Point ? 0 : l.type == LightType::Spot ? 1 : 2;
        if (ImGui::Combo("Type", &type, kTypes, 3))
            l.type = type == 0 ? LightType::Point : type == 1 ? LightType::Spot : LightType::Directional;

        ImGui::ColorEdit3("Colour", &l.color.x);
        ImGui::DragFloat("Intensity", &l.intensity, 0.05f, 0.0f, 10000.0f);
        if (l.type != LightType::Directional)
            ImGui::DragFloat("Range", &l.range, 0.1f, 0.1f, 1000.0f);
        if (l.type == LightType::Spot)
        {
            ImGui::SliderFloat("Outer cone", &l.outerConeDegrees, 1.0f, 89.0f, "%.0f deg");
            ImGui::SliderFloat("Inner cone", &l.innerConeDegrees, 0.0f, l.outerConeDegrees, "%.0f deg");
        }
        ImGui::Checkbox("Casts shadows", &l.castsShadows);
    }
}
