#include "dxpch.h"
#include "InspectorPanel.h"
#include "EditorIcons.h"
#include "EditorTheme.h"
#include "PropertyGrid.h"
#include "SceneEditing.h"
#include "scene/AnimationSystem.h"
#include "models/ModelAsset.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include "utils/ImageLoader.h"
#include "renderer/Renderer.h"
#include "PlatformDialogs.h"

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

        const char* const kBlendNames[]   = { "Opaque", "Alpha blend", "Additive", "Multiply", "Premultiplied alpha" };
        const char* const kCullNames[]    = { "None (double sided)", "Front faces", "Back faces" };
        const char* const kDepthNames[]   = { "Off", "Less", "Less or equal", "Greater", "Greater or equal", "Equal", "Not equal", "Always" };
        const char* const kChannelNames[] = { "R", "G", "B", "A" };

        // One row per texture slot: Browse / Replace and Clear. The image is decoded FOR THAT SLOT, so colour textures come
        // in as sRGB and data textures (normal, roughness, ...) as linear without the user having to think about it.
        void TextureRow(Material& m, TextureSlot slot, const char* label, const char* tooltip = nullptr)
        {
            PropertyGrid::Label(label, tooltip);
            ImGui::PushID(static_cast<int>(slot));
            const bool bound = m.GetTexture(slot) != nullptr;
            if (ImGui::Button(bound ? "Replace..." : "Browse..."))
            {
                const std::string path = PlatformDialogs::OpenImageFile();
                if (!path.empty())
                {
                    if (auto device = Renderer::GetDevice())
                        if (auto texture = ImageLoader::LoadForSlot(*device, path, slot))
                            m.SetTexture(slot, texture);
                }
            }
            if (bound)
            {
                ImGui::SameLine();
                if (ImGui::Button("Clear"))
                    m.SetTexture(slot, nullptr);
            }
            ImGui::PopID();
        }

        // Which channel of a data map carries its values (a glTF "metallicRoughness" image: roughness in G, metallic in B).
        void ChannelRow(Material& m, TextureSlot slot, const char* label)
        {
            if (!m.GetTexture(slot)) return;
            int channel = m.GetDataChannel(slot);
            if (PropertyGrid::Combo(label, channel, kChannelNames, 4))
                m.SetDataChannel(slot, channel);
        }

        void DrawMaterial(Material& m, int index, bool meshHasTangents)
        {
            MaterialProperties& p = m.GetProperties();       // read here, written only through the setters below
            const bool lit = m.GetShadingModel() == ShadingModel::Lit;
            const bool shaded = lit || m.GetShadingModel() == ShadingModel::Emissive || m.GetShadingModel() == ShadingModel::Unlit;

            // the title carries a colour swatch, so a long list can be told apart at a glance
            const std::string title = "     " + m.GetName() + "##mat" + std::to_string(index);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const bool open = ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
            const float h = ImGui::GetTextLineHeight();
            const float x0 = start.x + ImGui::GetTreeNodeToLabelSpacing();           // where the label begins: the swatch goes in the blank we left
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x0, start.y + 1.0f), ImVec2(x0 + h * 1.3f, start.y + 1.0f + h),
                                                      ImGui::ColorConvertFloat4ToU32(ImVec4(p.diffuseColor.x, p.diffuseColor.y, p.diffuseColor.z, 1.0f)), 3.0f);
            if (!open) return;

            // ---------------- surface ----------------
            if (PropertyGrid::Begin("material"))
            {
                PropertyGrid::Text("Shading", "%s", ShadingModelName(m.GetShadingModel()));

                DirectX::XMFLOAT4 diffuse = p.diffuseColor;
                if (PropertyGrid::Color4("Colour", &diffuse.x)) m.SetDiffuseColor(diffuse);

                if (lit)
                {
                    float metallic = p.metallic, roughness = p.roughness;
                    if (PropertyGrid::Slider("Metallic", metallic, 0.0f, 1.0f))   m.SetMetallic(metallic);
                    if (PropertyGrid::Slider("Roughness", roughness, 0.0f, 1.0f)) m.SetRoughness(roughness);

                    float occlusion = p.occlusionStrength;
                    if (PropertyGrid::Slider("Occlusion", occlusion, 0.0f, 1.0f)) m.SetOcculsionStrength(occlusion);

                    float normalStrength = p.normalScale;
                    if (PropertyGrid::Float("Normal strength", normalStrength, 0.01f, 0.0f, 4.0f)) m.SetNormalScale(normalStrength);

                    bool flipY = m.GetNormalFlipY();
                    if (PropertyGrid::Checkbox("Flip normal Y", flipY)) m.SetNormalFlipY(flipY);
                }

                if (shaded)
                {
                    DirectX::XMFLOAT4 emissive = p.emissiveColor;
                    if (PropertyGrid::Color3("Emissive", &emissive.x, true)) m.SetEmissiveColor(emissive);
                    float intensity = p.emissiveIntensity;
                    if (PropertyGrid::Float("Emissive power", intensity, 0.05f, 0.0f, 100.0f)) m.SetEmissiveIntensity(intensity);
                }

                float alpha = p.alpha;
                if (PropertyGrid::Slider("Alpha", alpha, 0.0f, 1.0f)) m.SetAlpha(alpha);
                PropertyGrid::End();
            }

            // ---------------- texture coordinates ----------------
            if (ImGui::TreeNodeEx("UV##uv", ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                if (PropertyGrid::Begin("uv"))
                {
                    float tu = p.textureScale.x, tv = p.textureScale.y;
                    const bool tilingChanged = PropertyGrid::Float("Tiling U", tu, 0.05f, 0.01f, 1000.0f) | PropertyGrid::Float("Tiling V", tv, 0.05f, 0.01f, 1000.0f);
                    if (tilingChanged) m.SetTextureScale({ tu, tv });

                    float ou = p.textureOffset.x, ov = p.textureOffset.y;
                    const bool offsetChanged = PropertyGrid::Float("Offset U", ou, 0.01f, -1000.0f, 1000.0f) | PropertyGrid::Float("Offset V", ov, 0.01f, -1000.0f, 1000.0f);
                    if (offsetChanged) m.SetTextureOffset({ ou, ov });
                    PropertyGrid::End();
                }
                ImGui::TreePop();
            }

            // ---------------- textures ----------------
            if (shaded && ImGui::TreeNodeEx("Textures##tex", ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                if (PropertyGrid::Begin("textures"))
                {
                    TextureRow(m, TextureSlot::Diffuse, "Base colour");
                    if (lit)
                    {
                        TextureRow(m, TextureSlot::Normal, "Normal");
                        TextureRow(m, TextureSlot::Specular, "Packed (ORM)", "R = occlusion, G = roughness, B = metallic (glTF style). Ignored when a roughness or metallic map is set.");
                        TextureRow(m, TextureSlot::Roughness, "Roughness");
                        ChannelRow(m, TextureSlot::Roughness, "  Roughness channel");
                        TextureRow(m, TextureSlot::Metallic, "Metallic");
                        ChannelRow(m, TextureSlot::Metallic, "  Metallic channel");
                        TextureRow(m, TextureSlot::AmbientOcclusion, "Occlusion");
                        ChannelRow(m, TextureSlot::AmbientOcclusion, "  Occlusion channel");
                        TextureRow(m, TextureSlot::Height, "Height (parallax)");
                        if (m.GetTexture(TextureSlot::Height))
                        {
                            float heightScale = p.heightScale;
                            if (PropertyGrid::Float("  Height scale", heightScale, 0.001f, 0.0f, 0.2f, "%.3f")) m.SetHeightScale(heightScale);
                        }
                        TextureRow(m, TextureSlot::DetailDiffuse, "Detail colour");
                        TextureRow(m, TextureSlot::DetailNormal, "Detail normal");
                        if (m.GetTexture(TextureSlot::DetailDiffuse) || m.GetTexture(TextureSlot::DetailNormal))
                        {
                            float du = p.detailScale.x, dv = p.detailScale.y;
                            const bool detailChanged = PropertyGrid::Float("  Detail tiling U", du, 0.05f, 0.01f, 1000.0f) | PropertyGrid::Float("  Detail tiling V", dv, 0.05f, 0.01f, 1000.0f);
                            if (detailChanged) m.SetDetailTextureScale({ du, dv });
                        }
                    }
                    TextureRow(m, TextureSlot::Emissive, "Emissive");
                    TextureRow(m, TextureSlot::Opacity, "Opacity");
                    PropertyGrid::End();
                }

                // The classic "my normal map does nothing": the shader needs tangents to use one.
                if (lit && m.GetTexture(TextureSlot::Normal) && !meshHasTangents)
                {
                    ImGui::PushTextWrapPos();
                    ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "This mesh has no tangents, so its normal map is ignored.");
                    ImGui::PopTextWrapPos();
                }
                ImGui::TreePop();
            }

            // ---------------- render state ----------------
            if (shaded && ImGui::TreeNodeEx("Render state##state", ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                if (PropertyGrid::Begin("state"))
                {
                    RenderState state = m.GetRenderState();
                    int blend = static_cast<int>(state.blend);
                    int cull  = static_cast<int>(state.cull);
                    int depth = static_cast<int>(state.depthTest);
                    bool changed = false;
                    if (PropertyGrid::Combo("Blending", blend, kBlendNames, 5))  { state.blend = static_cast<RHI::BlendMode>(blend);        changed = true; }
                    if (PropertyGrid::Combo("Culling", cull, kCullNames, 3))     { state.cull = static_cast<RHI::CullMode>(cull);           changed = true; }
                    if (PropertyGrid::Combo("Depth test", depth, kDepthNames, 8)){ state.depthTest = static_cast<RHI::DepthTestMode>(depth); changed = true; }
                    if (changed) m.SetRenderState(state);
                    PropertyGrid::End();
                }
                ImGui::TreePop();
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
        if (e.Has<AnimatorComponent>())     DrawAnimator(e);

        const EditorTheme::Palette& pal = EditorTheme::Colors();
        ImGui::PushStyleColor(ImGuiCol_Text, pal.textDim);
        if (e.Has<SkyboxComponent>())            ImGui::TextUnformatted("Skybox: not editable yet");
        if (e.Has<SkinnedMeshComponent>())
        {
            // its bones come from the Animator above it: say whose
            const AnimatorComponent* animator = AnimationSystem::FindAnimator(*e.GetScene(), e.Id());
            ImGui::TextUnformatted(animator ? "Skinned mesh: drawn with the bones of the Animator above it"
                                            : "Skinned mesh: no Animator above it, drawn without bones");
        }
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
            if (ImGui::BeginMenu("Mesh renderer", !e.Has<MeshRendererComponent>()))
            {
                // the primitives are the meshes the engine can make itself; imported models arrive with the asset milestone
                static const SceneEditing::Primitive kPrimitives[] = {
                    SceneEditing::Primitive::Cube, SceneEditing::Primitive::Sphere, SceneEditing::Primitive::Plane };
                for (SceneEditing::Primitive p : kPrimitives)
                    if (ImGui::MenuItem(SceneEditing::PrimitiveName(p)) && ctx.scene)
                        SceneEditing::AddMeshRenderer(*ctx.scene, ctx.device, e, p);

                // the meshes of the imported models. Skinned ones are left out: they need the skeleton and the Animator
                // of their model, which "Add to scene" in the Assets panel brings along.
                ImGui::Separator();
                if (ctx.models.Count() == 0)
                    ImGui::TextDisabled("Import a model to use its meshes");
                for (const ImportedModel& model : ctx.models.All())
                {
                    if (!ImGui::BeginMenu(model.name.c_str())) continue;
                    bool any = false;
                    for (const ModelMesh& entry : model.asset->meshes)
                    {
                        if (entry.skinned || !entry.mesh) continue;
                        any = true;
                        if (ImGui::MenuItem(entry.name.c_str()) && ctx.scene)
                            SceneEditing::AddMeshRenderer(*ctx.scene, e, entry.mesh);
                    }
                    if (!any) ImGui::TextDisabled("(no unskinned meshes)");
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
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
        bool meshHasTangents = true;
        if (mr.mesh && mr.mesh->GetResource())
            if (const VertexData* vertexData = mr.mesh->GetResource()->GetVertexData())
                meshHasTangents = vertexData->GetLayout().HasAttribute(RHI::VertexAttributeType::Tangent);
        for (size_t i = 0; i < mr.materials.size(); ++i)
            if (mr.materials[i])
                DrawMaterial(*mr.materials[i], static_cast<int>(i), meshHasTangents);
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

    // The Animator: which clip plays, how fast, how it repeats, and where it is. The buttons and fields only change the
    // component; AnimationSystem applies them on the next update (so this also works while the scene is paused).
    void InspectorPanel::DrawAnimator(Entity e)
    {
        if (!Section("Animator", nullptr))
            return;

        AnimatorComponent& a = e.Get<AnimatorComponent>();
        if (PropertyGrid::Begin("animator"))
        {
            if (a.clips.empty())
            {
                PropertyGrid::Text("Clip", "this model has no animation");
            }
            else
            {
                std::vector<std::string> names;
                names.reserve(a.clips.size());
                for (size_t i = 0; i < a.clips.size(); ++i)
                    names.push_back(a.clips[i] && !a.clips[i]->GetName().empty() ? a.clips[i]->GetName() : "Clip " + std::to_string(i));
                std::vector<const char*> items;
                for (const std::string& n : names) items.push_back(n.c_str());

                int clip = std::clamp(a.clip, 0, static_cast<int>(items.size()) - 1);
                if (PropertyGrid::Combo("Clip", clip, items.data(), static_cast<int>(items.size())))
                    AnimationSystem::SetClip(a, clip);
            }

            PropertyGrid::Float("Speed", a.speed, 0.01f, -4.0f, 4.0f, "%.2fx");

            static const char* kModes[] = { "Once", "Loop", "Ping-pong" };
            int mode = static_cast<int>(a.mode);
            if (PropertyGrid::Combo("Repeat", mode, kModes, 3))
                a.mode = static_cast<PlaybackMode>(mode);

            if (a.clip >= 0 && static_cast<size_t>(a.clip) < a.clips.size() && a.clips[a.clip])
            {
                const float duration = a.clips[a.clip]->GetDurationSeconds();
                float time = a.time;
                if (duration > 0.0f && PropertyGrid::Slider("Time", time, 0.0f, duration, "%.2f"))
                    a.time = time;                           // scrubbing: the system follows it
            }
            if (a.skeleton)
                PropertyGrid::Text("Bones", "%zu", a.skeleton->GetBoneCount());
            PropertyGrid::End();
        }

        if (ImGui::Button(a.playing ? "Pause" : "Play"))
        {
            if (a.playing) AnimationSystem::Pause(a); else AnimationSystem::Play(a);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            AnimationSystem::Stop(a);
        ImGui::TreePop();
    }
}
