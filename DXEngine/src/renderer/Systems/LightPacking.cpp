#include "dxpch.h"
#include "LightPacking.h"
#include "scene/Scene.h"
#include <algorithm>
#include <cmath>

namespace DXEngine
{
    // The GPU numbers lights the way the scene does (the HLSL compares against these values).
    static_assert(static_cast<uint32_t>(LightType::Directional) == GpuLight_Directional &&
                  static_cast<uint32_t>(LightType::Point)       == GpuLight_Point &&
                  static_cast<uint32_t>(LightType::Spot)        == GpuLight_Spot,
                  "LightType and GpuLightType must use the same numbers");

    namespace
    {
        float DistanceSq(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
        {
            const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            return dx * dx + dy * dy + dz * dz;
        }
    }

    uint32_t PackLights(Scene* scene, const RenderEnvironment& env, const DirectX::XMFLOAT3& cameraPosition,
                        SceneLightData& out, std::vector<LightGPU>& localLights, uint32_t maxLocalLights)
    {
        out = SceneLightData{};
        out.ambientColor     = env.ambientColor;
        out.ambientIntensity = env.ambientIntensity;
        out.iblIntensity     = env.iblIntensity;
        out.exposure         = env.exposure;
        out.gamma            = env.gamma;

        localLights.clear();                                          // keeps the capacity
        if (!scene) return 0;
        entt::registry& reg = scene->Registry();

        for (auto [entity, transform, light] : reg.view<TransformComponent, LightComponent>().each())
        {
            (void)entity;
            if (!(light.intensity > 0.0f)) continue;                  // switched off (also rejects NaN)

            switch (light.type)
            {
            case LightType::Directional:
            {
                if (out.directionalLightCount >= SceneLightData::MAX_DIRECTIONAL_LIGHTS) break;
                DirectionalLightGPU& g = out.directionalLights[out.directionalLightCount++];
                g.direction       = transform.WorldForward();         // the way the light travels
                g.intensity       = light.intensity;
                g.color           = light.color;
                g.shadowMapIndex  = -1.0f;                            // a ShadowPass assigns real indices (shadows milestone)
                g.cascadeSplits   = { 0.1f, 0.25f, 0.5f, 1.0f };
                for (auto& m : g.shadowMatrices) m = DirectX::XMMatrixIdentity();
                break;
            }
            case LightType::Point:
            case LightType::Spot:
            {
                if (!(light.range > 0.0f)) break;                     // no reach: it can never light anything

                LightGPU g{};
                g.position    = transform.WorldPosition();
                g.range       = light.range;
                g.color       = { light.color.x * light.intensity, light.color.y * light.intensity, light.color.z * light.intensity };
                g.type        = static_cast<uint32_t>(light.type);
                g.shadowIndex = -1;                                   // the shadow milestone assigns real indices

                if (light.type == LightType::Spot)
                {
                    const float outer = std::max(light.outerConeDegrees, 0.1f);
                    const float inner = std::min(light.innerConeDegrees, outer);
                    const float cosInner = std::cos(DirectX::XMConvertToRadians(inner));
                    const float cosOuter = std::cos(DirectX::XMConvertToRadians(outer));

                    // cone(cosAngle) = saturate(cosAngle * scale + offset): 0 at the outer cone, 1 at the inner cone.
                    // The floor on the width keeps scale finite when inner == outer (a hard-edged spot), where the
                    // old shader divided by zero.
                    const float width = std::max(cosInner - cosOuter, 1e-4f);
                    g.direction  = transform.WorldForward();
                    g.spotScale  = 1.0f / width;
                    g.spotOffset = -cosOuter * g.spotScale;
                }
                localLights.push_back(g);
                break;
            }
            }
        }

        // Over the limit: keep the lights nearest to the camera.
        uint32_t dropped = 0;
        if (localLights.size() > maxLocalLights)
        {
            dropped = static_cast<uint32_t>(localLights.size()) - maxLocalLights;
            std::nth_element(localLights.begin(), localLights.begin() + maxLocalLights, localLights.end(),
                [&](const LightGPU& a, const LightGPU& b)
                { return DistanceSq(a.position, cameraPosition) < DistanceSq(b.position, cameraPosition); });
            localLights.resize(maxLocalLights);
        }

        out.localLightCount = static_cast<uint32_t>(localLights.size());
        return dropped;
    }

    uint32_t LightBufferCapacityFor(uint32_t lightCount)
    {
        uint32_t capacity = kMinLocalLightCapacity;
        while (capacity < lightCount && capacity < kMaxLocalLights)
            capacity *= 2;
        return std::min(capacity, kMaxLocalLights);
    }
}
