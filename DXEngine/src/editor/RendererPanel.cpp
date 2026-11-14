#include "dxpch.h"
#include "RendererPanel.h"
#include "EditorTheme.h"
#include "PropertyGrid.h"
#include "renderer/Renderer.h"
#include "renderer/Core/RenderTypes.h"

#include "imgui.h"

namespace DXEngine
{
    namespace
    {
        // Resolutions the renderer rounds to anyway (powers of two).
        bool ResolutionRow(const char* label, uint32_t& value)
        {
            static const uint32_t kSizes[] = { 128, 256, 512, 1024, 2048, 4096 };
            PropertyGrid::Label(label);
            ImGui::PushID(label);
            bool changed = false;
            char preview[16];
            snprintf(preview, sizeof(preview), "%u", value);
            if (ImGui::BeginCombo("##v", preview))
            {
                for (uint32_t size : kSizes)
                {
                    char item[16];
                    snprintf(item, sizeof(item), "%u", size);
                    if (ImGui::Selectable(item, value == size)) { value = size; changed = true; }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
            return changed;
        }

        bool IntSlider(const char* label, uint32_t& value, int min, int max)
        {
            PropertyGrid::Label(label);
            ImGui::PushID(label);
            int v = static_cast<int>(value);
            const bool changed = ImGui::SliderInt("##v", &v, min, max);
            if (changed) value = static_cast<uint32_t>(v);
            ImGui::PopID();
            return changed;
        }

        // a counter that must be zero in a healthy scene: red when it is not
        void Warn(const char* label, uint32_t count)
        {
            if (count == 0) return;
            PropertyGrid::Label(label);
            ImGui::TextColored(EditorTheme::Colors().danger, "%u", count);
        }

        bool DrawEnvironment(RenderEnvironment& env)
        {
            bool changed = false;
            if (PropertyGrid::Begin("environment"))
            {
                changed |= PropertyGrid::Color3("Ambient colour", &env.ambientColor.x);
                changed |= PropertyGrid::Slider("Ambient", env.ambientIntensity, 0.0f, 2.0f);
                changed |= PropertyGrid::Slider("IBL", env.iblIntensity, 0.0f, 4.0f);
                changed |= PropertyGrid::Slider("Exposure", env.exposure, 0.05f, 4.0f);
                changed |= PropertyGrid::Slider("Gamma", env.gamma, 1.0f, 3.0f);

                static const char* kModes[] = { "None (every pixel loops all lights)", "Clustered", "Cluster heat map" };
                int mode = static_cast<int>(env.lightCulling);
                if (PropertyGrid::Combo("Light culling", mode, kModes, 3))
                {
                    env.lightCulling = static_cast<LightCullingMode>(mode);
                    changed = true;
                }
                PropertyGrid::End();
            }
            return changed;
        }

        bool DrawShadows(ShadowSettings& sh)
        {
            bool changed = false;
            if (PropertyGrid::Begin("shadows"))
            {
                changed |= PropertyGrid::Checkbox("Enabled", sh.enabled);
                PropertyGrid::Text("Atlas", "%u x %u (set in code)", sh.atlasSize, sh.atlasSize);
                PropertyGrid::End();
            }
            ImGui::BeginDisabled(!sh.enabled);

            ImGui::SeparatorText("Directional (cascades)");
            if (PropertyGrid::Begin("cascades"))
            {
                changed |= IntSlider("Cascades", sh.cascadeCount, 1, 4);
                changed |= ResolutionRow("Resolution", sh.cascadeResolution);
                changed |= PropertyGrid::Float("Max distance", sh.maxDistance, 1.0f, 5.0f, 500.0f, "%.0f m");
                changed |= PropertyGrid::Slider("Split lambda", sh.cascadeLambda, 0.0f, 1.0f);
                changed |= PropertyGrid::Slider("Fade", sh.fadeFraction, 0.0f, 0.5f);
                changed |= PropertyGrid::Checkbox("Snap to texels", sh.snapToTexels);
                PropertyGrid::End();
            }

            ImGui::SeparatorText("Local lights");
            if (PropertyGrid::Begin("local"))
            {
                changed |= IntSlider("Max shadowed", sh.maxShadowedLocalLights, 0, 32);
                changed |= ResolutionRow("Spot resolution", sh.spotResolution);
                changed |= ResolutionRow("Point resolution", sh.pointFaceResolution);
                PropertyGrid::End();
            }

            ImGui::SeparatorText("Quality and bias");
            if (PropertyGrid::Begin("bias"))
            {
                changed |= IntSlider("PCF radius", sh.pcfRadius, 0, 3);
                changed |= PropertyGrid::Slider("Normal bias", sh.normalBias, 0.0f, 5.0f);
                changed |= PropertyGrid::Slider("Constant bias", sh.constantBias, 0.0f, 10.0f);
                PropertyGrid::Label("Raster depth bias");
                ImGui::PushID("rdb");
                changed |= ImGui::SliderInt("##v", &sh.rasterDepthBias, 0, 2000);
                ImGui::PopID();
                changed |= PropertyGrid::Slider("Raster slope bias", sh.rasterSlopeBias, 0.0f, 8.0f);
                changed |= PropertyGrid::Float("Caster extent", sh.casterExtent, 1.0f, 10.0f, 500.0f, "%.0f m");
                PropertyGrid::End();
            }
            ImGui::EndDisabled();
            return changed;
        }
    }

    void RendererPanel::OnImGui(bool* open)
    {
        if (open && !*open) return;
        if (!ImGui::Begin("Renderer", open))
        {
            ImGui::End();
            return;
        }

        RenderEnvironment env = Renderer::GetEnvironment();
        bool changed = false;

        if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen) && PropertyGrid::Begin("display"))
        {
            bool wireframe = Renderer::IsWireframeEnabled();
            if (PropertyGrid::Checkbox("Wireframe", wireframe)) Renderer::EnableWireframe(wireframe);
            bool culling = Renderer::IsFrustumCullingEnabled();
            if (PropertyGrid::Checkbox("Frustum culling", culling)) Renderer::EnableFrustumCulling(culling);
            PropertyGrid::End();
        }

        if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
            changed |= DrawEnvironment(env);

        if (ImGui::CollapsingHeader("Shadows"))
            changed |= DrawShadows(env.shadows);

        if (ImGui::CollapsingHeader("Statistics (last frame)"))
        {
            const RenderStatistics& st = Renderer::GetStats();
            if (PropertyGrid::Begin("stats"))
            {
                PropertyGrid::Text("Frame", "%.1f FPS  (%.2f ms)", ImGui::GetIO().Framerate, ImGui::GetIO().Framerate > 0.0f ? 1000.0f / ImGui::GetIO().Framerate : 0.0f);
                PropertyGrid::Text("Objects", "%u drawn, %u culled", st.objectsSubmitted, st.objectsCulled);
                PropertyGrid::Text("Draw calls", "%u", st.drawCalls);
                PropertyGrid::Text("Triangles", "%u", st.trianglesRendered);
                PropertyGrid::Text("State changes", "%u pipeline, %u material", st.pipelineChanges, st.materialChanges);
                PropertyGrid::Text("Lights", "%u directional, %u local", st.directionalLights, st.localLights);
                if (st.clusterAssignments)
                    PropertyGrid::Text("Clusters", "%u assignments, max %u in one", st.clusterAssignments, st.clusterMaxLights);
                if (st.shadowViews)
                    PropertyGrid::Text("Shadows", "%u directional, %u local, %u views", st.shadowedDirectional, st.shadowedLocal, st.shadowViews);
                Warn("Lights dropped", st.lightsDropped);
                Warn("Cluster pairs dropped", st.clusterDropped);
                Warn("Shadow lights dropped", st.shadowLightsDropped);
                PropertyGrid::End();
            }
        }

        if (changed)
            Renderer::SetEnvironment(env);

        ImGui::End();
    }
}
