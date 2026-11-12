#include "dxpch.h"
#include "RendererPanel.h"
#include "renderer/Renderer.h"
#include "renderer/Core/RenderTypes.h"

#include "imgui.h"

namespace DXEngine
{
    namespace
    {
        // Resolutions the renderer rounds to anyway (powers of two).
        bool ResolutionCombo(const char* label, uint32_t& value)
        {
            static const uint32_t kSizes[] = { 128, 256, 512, 1024, 2048, 4096 };
            bool changed = false;
            char preview[16];
            snprintf(preview, sizeof(preview), "%u", value);
            if (ImGui::BeginCombo(label, preview))
            {
                for (uint32_t size : kSizes)
                {
                    char item[16];
                    snprintf(item, sizeof(item), "%u", size);
                    if (ImGui::Selectable(item, value == size)) { value = size; changed = true; }
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        // A red line for the counters that must be zero in a healthy scene.
        void WarnIfNonZero(const char* text, uint32_t count)
        {
            if (count > 0)
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "%s: %u", text, count);
        }

        bool DrawEnvironment(RenderEnvironment& env)
        {
            bool changed = false;
            changed |= ImGui::ColorEdit3("Ambient colour", &env.ambientColor.x);
            changed |= ImGui::SliderFloat("Ambient intensity", &env.ambientIntensity, 0.0f, 2.0f);
            changed |= ImGui::SliderFloat("IBL intensity", &env.iblIntensity, 0.0f, 4.0f);
            changed |= ImGui::SliderFloat("Exposure", &env.exposure, 0.05f, 4.0f);
            changed |= ImGui::SliderFloat("Gamma", &env.gamma, 1.0f, 3.0f);

            static const char* kModes[] = { "None (every pixel loops all lights)", "Clustered", "Cluster heat map" };
            int mode = static_cast<int>(env.lightCulling);
            if (ImGui::Combo("Light culling", &mode, kModes, 3))
            {
                env.lightCulling = static_cast<LightCullingMode>(mode);
                changed = true;
            }
            return changed;
        }

        bool DrawShadows(ShadowSettings& sh)
        {
            bool changed = false;
            changed |= ImGui::Checkbox("Shadows enabled", &sh.enabled);
            ImGui::BeginDisabled(!sh.enabled);

            ImGui::TextDisabled("Atlas %ux%u (set in code: changing it reallocates the depth texture)", sh.atlasSize, sh.atlasSize);

            if (ImGui::TreeNodeEx("Directional (cascades)", ImGuiTreeNodeFlags_DefaultOpen))
            {
                int cascades = static_cast<int>(sh.cascadeCount);
                if (ImGui::SliderInt("Cascades", &cascades, 1, 4)) { sh.cascadeCount = static_cast<uint32_t>(cascades); changed = true; }
                changed |= ResolutionCombo("Cascade resolution", sh.cascadeResolution);
                changed |= ImGui::SliderFloat("Max distance", &sh.maxDistance, 5.0f, 500.0f);
                changed |= ImGui::SliderFloat("Split lambda", &sh.cascadeLambda, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Fade fraction", &sh.fadeFraction, 0.0f, 0.5f);
                changed |= ImGui::Checkbox("Snap to texels", &sh.snapToTexels);
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Local lights", ImGuiTreeNodeFlags_DefaultOpen))
            {
                int maxLocal = static_cast<int>(sh.maxShadowedLocalLights);
                if (ImGui::SliderInt("Max shadowed lights", &maxLocal, 0, 32)) { sh.maxShadowedLocalLights = static_cast<uint32_t>(maxLocal); changed = true; }
                changed |= ResolutionCombo("Spot resolution", sh.spotResolution);
                changed |= ResolutionCombo("Point face resolution", sh.pointFaceResolution);
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("Quality / bias", ImGuiTreeNodeFlags_DefaultOpen))
            {
                int pcf = static_cast<int>(sh.pcfRadius);
                if (ImGui::SliderInt("PCF radius", &pcf, 0, 3)) { sh.pcfRadius = static_cast<uint32_t>(pcf); changed = true; }
                changed |= ImGui::SliderFloat("Normal bias (texels)", &sh.normalBias, 0.0f, 5.0f);
                changed |= ImGui::SliderFloat("Constant bias (texels)", &sh.constantBias, 0.0f, 10.0f);
                changed |= ImGui::SliderInt("Raster depth bias", &sh.rasterDepthBias, 0, 2000);
                changed |= ImGui::SliderFloat("Raster slope bias", &sh.rasterSlopeBias, 0.0f, 8.0f);
                changed |= ImGui::SliderFloat("Caster extent", &sh.casterExtent, 10.0f, 500.0f);
                ImGui::TreePop();
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

        // ---- frame ----
        const float fps = ImGui::GetIO().Framerate;
        ImGui::Text("%.1f FPS  (%.2f ms)", fps, fps > 0.0f ? 1000.0f / fps : 0.0f);

        // ---- toggles (the renderer owns the state: ask it, so a key that flips the same switch stays in sync) ----
        bool wireframe = Renderer::IsWireframeEnabled();
        if (ImGui::Checkbox("Wireframe", &wireframe)) Renderer::EnableWireframe(wireframe);
        ImGui::SameLine();
        bool culling = Renderer::IsFrustumCullingEnabled();
        if (ImGui::Checkbox("Frustum culling", &culling)) Renderer::EnableFrustumCulling(culling);

        // ---- statistics of the last finished frame ----
        if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const RenderStatistics& st = Renderer::GetStats();
            ImGui::Text("Objects   %u submitted, %u culled", st.objectsSubmitted, st.objectsCulled);
            ImGui::Text("Draw calls %u   Triangles %u", st.drawCalls, st.trianglesRendered);
            ImGui::Text("Pipeline changes %u   Material changes %u", st.pipelineChanges, st.materialChanges);
            ImGui::Text("Lights    %u directional, %u local", st.directionalLights, st.localLights);
            WarnIfNonZero("Lights dropped (over the limit)", st.lightsDropped);
            if (st.clusterAssignments)
                ImGui::Text("Clusters  %u assignments, at most %u lights in one", st.clusterAssignments, st.clusterMaxLights);
            WarnIfNonZero("Cluster pairs dropped (index list full)", st.clusterDropped);
            if (st.shadowViews || st.shadowLightsDropped)
                ImGui::Text("Shadows   %u directional, %u local, %u views, %u caster draws",
                            st.shadowedDirectional, st.shadowedLocal, st.shadowViews, st.shadowCasterDraws);
            WarnIfNonZero("Shadow lights dropped (over budget / atlas full)", st.shadowLightsDropped);
        }

        // ---- environment + shadows: read the live values, write back only when something was edited ----
        RenderEnvironment env = Renderer::GetEnvironment();
        bool changed = false;
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen))
            changed |= DrawEnvironment(env);
        if (ImGui::CollapsingHeader("Shadows"))
            changed |= DrawShadows(env.shadows);
        if (changed)
            Renderer::SetEnvironment(env);

        ImGui::End();
    }
}
