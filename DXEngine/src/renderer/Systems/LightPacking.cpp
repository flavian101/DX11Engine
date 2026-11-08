#include "dxpch.h"
#include "LightPacking.h"
#include "scene/Scene.h"
#include <algorithm>
#include <cmath>

namespace DXEngine
{
    namespace
    {
        struct Candidate { float distanceSq; entt::entity entity; };

        float DistanceSq(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
        {
            const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            return dx * dx + dy * dy + dz * dz;
        }

        DirectX::XMFLOAT4X4 IdentityMatrix() { return TransformComponent::Identity(); }
    }

    void PackLights(Scene* scene, const RenderEnvironment& env, const DirectX::XMFLOAT3& cameraPosition, SceneLightData& out)
    {
        out = SceneLightData{};
        out.ambientColor     = env.ambientColor;
        out.ambientIntensity = env.ambientIntensity;
        out.iblIntensity     = env.iblIntensity;
        out.exposure         = env.exposure;
        out.gamma            = env.gamma;

        if (!scene) return;
        entt::registry& reg = scene->Registry();

        std::vector<Candidate> points, spots;

        for (auto [entity, transform, light] : reg.view<TransformComponent, LightComponent>().each())
        {
            if (light.intensity <= 0.0f) continue;                    // switched off

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
                points.push_back({ DistanceSq(transform.WorldPosition(), cameraPosition), entity });
                break;
            case LightType::Spot:
                spots.push_back({ DistanceSq(transform.WorldPosition(), cameraPosition), entity });
                break;
            }
        }

        auto byDistance = [](const Candidate& a, const Candidate& b) { return a.distanceSq < b.distanceSq; };
        std::sort(points.begin(), points.end(), byDistance);
        std::sort(spots.begin(), spots.end(), byDistance);

        // 1 / (1 + d^2), windowed to zero at 'range' by the shader. (The old three hand-tuned numbers are gone;
        // intensity is the only brightness control now.)
        const DirectX::XMFLOAT3 attenuation{ 1.0f, 0.0f, 1.0f };

        for (const Candidate& c : points)
        {
            if (out.pointLightCount >= SceneLightData::MAX_POINT_LIGHTS) break;
            const auto& t = reg.get<TransformComponent>(c.entity);
            const auto& l = reg.get<LightComponent>(c.entity);
            PointLightGPU& g = out.pointLights[out.pointLightCount++];
            g.position       = t.WorldPosition();
            g.radius         = l.range;
            g.color          = l.color;
            g.intensity      = l.intensity;
            g.attenuation    = attenuation;
            g.shadowMapIndex = -1.0f;
        }

        for (const Candidate& c : spots)
        {
            if (out.spotLightCount >= SceneLightData::MAX_SPOT_LIGHTS) break;
            const auto& t = reg.get<TransformComponent>(c.entity);
            const auto& l = reg.get<LightComponent>(c.entity);
            const float outer = std::max(l.outerConeDegrees, 0.1f);
            const float inner = std::min(l.innerConeDegrees, outer);
            SpotLightGPU& g = out.spotLights[out.spotLightCount++];
            g.position       = t.WorldPosition();
            g.range          = l.range;
            g.direction      = t.WorldForward();
            g.innerCone      = std::cos(DirectX::XMConvertToRadians(inner));   // the shader compares cosines
            g.color          = l.color;
            g.outerCone      = std::cos(DirectX::XMConvertToRadians(outer));
            g.intensity      = l.intensity;
            g.attenuation    = attenuation;
            g.shadowMapIndex = -1.0f;
            g.shadowMatrix   = DirectX::XMMatrixIdentity();
        }
    }
}
