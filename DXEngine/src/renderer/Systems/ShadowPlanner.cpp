#include "dxpch.h"
#include "ShadowPlanner.h"
#include "LightClustering.h"      // ProjectionIsPerspective / near / far
#include "scene/Scene.h"
#include <algorithm>
#include <cmath>

namespace DXEngine
{
    using namespace DirectX;
    using ShadowMath::AtlasRect;

    namespace
    {
        uint32_t EntityId(entt::entity e) { return static_cast<uint32_t>(entt::to_integral(e)); }

        // a power of two, clamped to [minSize, atlas]
        uint32_t TileSize(uint32_t wanted, uint32_t minSize, uint32_t atlas)
        {
            const uint32_t pow2 = ShadowMath::FloorPow2(std::max(wanted, 1u));
            return std::clamp(pow2, std::min(minSize, atlas), atlas);
        }
    }

    uint32_t ShadowPlanner::AllocateTiles(uint32_t count, uint32_t size, uint32_t minSize, AtlasRect* rects)
    {
        for (; size >= minSize && size > 0; size /= 2)
        {
            ShadowMath::AtlasAllocator trial = m_Atlas;           // all-or-nothing: try on a copy, keep it only if all fit
            bool ok = true;
            for (uint32_t i = 0; i < count && ok; ++i)
                ok = trial.Allocate(size, rects[i]);
            if (ok) { m_Atlas = trial; return size; }
        }
        return 0;
    }

    void ShadowPlanner::Plan(Scene* scene, const ExtractionCamera& camera, const ShadowSettings& settings,
                             const SceneExtraction& extraction, ShadowFrame& out, RenderStatistics& stats)
    {
        out.Clear();
        m_Volumes.clear();
        m_Candidates.clear();
        if (!scene || !settings.enabled) return;

        m_Atlas.Reset(settings.atlasSize, settings.minTileResolution);
        out.atlasSize = m_Atlas.Size();
        if (out.atlasSize == 0) return;

        entt::registry& reg = scene->Registry();
        const bool usableCamera = ProjectionIsPerspective(camera.projection);
        const BoundingFrustum cameraFrustum = SceneExtraction::BuildFrustum(camera);

        // ---- 1. collect the lights that want shadows
        struct Directional { uint32_t id; XMFLOAT3 direction; float intensity; };
        std::vector<Directional> directional;

        for (auto [entity, transform, light] : reg.view<TransformComponent, LightComponent>().each())
        {
            if (!light.castsShadows || !(light.intensity > 0.0f)) continue;   // (same "switched off" rule as PackLights)

            if (light.type == LightType::Directional)
            {
                directional.push_back({ EntityId(entity), transform.WorldForward(), light.intensity });
                continue;
            }
            if (!(light.range > 0.0f)) continue;

            LocalCandidate c;
            c.id           = EntityId(entity);
            c.isSpot       = light.type == LightType::Spot;
            c.position     = transform.WorldPosition();
            c.direction    = transform.WorldForward();
            c.range        = light.range;
            c.outerRadians = XMConvertToRadians(std::max(light.outerConeDegrees, 0.1f));

            // a light whose reach never touches the view cannot shade a visible pixel: no shadow needed
            if (usableCamera && cameraFrustum.Contains(BoundingSphere(c.position, c.range)) == DISJOINT) continue;

            const float dx = c.position.x - camera.position.x, dy = c.position.y - camera.position.y, dz = c.position.z - camera.position.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            c.importance = c.range / std::max(dist, 0.5f);                       // roughly: how big the light looks
            m_Candidates.push_back(c);
        }

        // ---- 2. directional lights: the strongest first
        std::sort(directional.begin(), directional.end(), [](const Directional& a, const Directional& b)
        {
            return a.intensity != b.intensity ? a.intensity > b.intensity : a.id < b.id;
        });
        const uint32_t dirBudget = std::min(settings.maxShadowedDirectional, SceneLightData::MAX_DIRECTIONAL_LIGHTS);
        for (size_t i = 0; i < directional.size(); ++i)
        {
            if (i >= dirBudget || !usableCamera || !PlanDirectional(directional[i].id, directional[i].direction, camera, settings, out))
                ++stats.shadowLightsDropped;
            else
                ++stats.shadowedDirectional;
        }

        // ---- 3. local lights: the most important first
        std::sort(m_Candidates.begin(), m_Candidates.end(), [](const LocalCandidate& a, const LocalCandidate& b)
        {
            return a.importance != b.importance ? a.importance > b.importance : a.id < b.id;
        });
        for (size_t i = 0; i < m_Candidates.size(); ++i)
        {
            if (i >= settings.maxShadowedLocalLights || !PlanLocal(m_Candidates[i], settings, out))
                ++stats.shadowLightsDropped;
            else
                ++stats.shadowedLocal;
        }

        stats.shadowViews = static_cast<uint32_t>(out.views.size());
        if (out.views.empty()) return;

        // ---- 4. casters: extracted once, then assigned to every view they can reach
        extraction.ExtractShadowCasters(*scene, out.casters, m_CasterBounds);
        for (size_t v = 0; v < out.views.size(); ++v)
            for (uint32_t c = 0; c < m_CasterBounds.size(); ++c)
                if (m_Volumes[v].Reaches(m_CasterBounds[c]))
                    out.views[v].casters.push_back(c);
    }

    bool ShadowPlanner::PlanDirectional(uint32_t id, const XMFLOAT3& direction, const ExtractionCamera& camera,
                                        const ShadowSettings& s, ShadowFrame& out)
    {
        const uint32_t cascades = std::clamp(s.cascadeCount, 1u, kMaxShadowCascades);
        const float nearZ = ProjectionNearPlane(camera.projection);
        const float farZ  = std::min(ProjectionFarPlane(camera.projection), s.maxDistance);
        if (!(farZ > nearZ + 0.01f)) return false;

        AtlasRect rects[kMaxShadowCascades];
        const uint32_t size = AllocateTiles(cascades, TileSize(s.cascadeResolution, s.minTileResolution, out.atlasSize),
                                            std::min(s.minTileResolution, out.atlasSize), rects);
        if (size == 0) return false;

        float splits[kMaxShadowCascades];
        ShadowMath::ComputeCascadeSplits(nearZ, farZ, cascades, s.cascadeLambda, splits);

        const float tanHalfX = 1.0f / camera.projection._11;
        const float tanHalfY = 1.0f / camera.projection._22;

        ShadowLightAssignment a;
        a.firstView = static_cast<int32_t>(out.views.size());
        a.viewCount = cascades;
        float padded[kMaxShadowCascades];
        for (uint32_t i = 0; i < kMaxShadowCascades; ++i) padded[i] = splits[std::min(i, cascades - 1)];   // unused entries repeat the last
        a.cascadeSplits = { padded[0], padded[1], padded[2], padded[3] };
        a.fadeStart = splits[cascades - 1] * (1.0f - std::clamp(s.fadeFraction, 0.0f, 0.5f));

        for (uint32_t i = 0; i < cascades; ++i)
        {
            const float sliceNear = i == 0 ? nearZ : splits[i - 1];
            const ShadowMath::CascadeFit fit = ShadowMath::FitCascade(camera.view, tanHalfX, tanHalfY, sliceNear, splits[i],
                                                                      direction, size, s.casterExtent, s.snapToTexels);
            ShadowView view;
            view.kind = ShadowView::Kind::Cascade;
            view.lightId = id;
            view.viewProj = fit.viewProj;
            view.x = rects[i].x; view.y = rects[i].y; view.size = rects[i].size;
            out.views.push_back(std::move(view));

            // constant bias in NDC depth: (texels * world size of a texel) / depth range of the orthographic box
            const float depthBias = s.constantBias * fit.texelWorldSize / std::max(fit.depthRange, 1e-3f);
            out.gpuViews.push_back(ShadowMath::MakeGpuView(fit.viewProj, rects[i], out.atlasSize, s.normalBias, fit.texelWorldSize, depthBias, false));

            CullVolume vol;
            vol.isBox = true;
            vol.box = fit.cullBox;
            m_Volumes.push_back(vol);
        }
        out.lights[id] = a;
        return true;
    }

    bool ShadowPlanner::PlanLocal(const LocalCandidate& c, const ShadowSettings& s, ShadowFrame& out)
    {
        const uint32_t count   = c.isSpot ? 1u : 6u;
        const uint32_t pcf     = std::min(s.pcfRadius, 3u);
        const uint32_t maxSize = c.isSpot ? s.spotResolution : s.pointFaceResolution;

        // bigger tiles for lights that look big on screen, smaller for far ones
        uint32_t wanted = maxSize;
        if (c.importance < 0.35f)      wanted = maxSize / 4;
        else if (c.importance < 1.0f)  wanted = maxSize / 2;

        AtlasRect rects[6];
        const uint32_t size = AllocateTiles(count, TileSize(wanted, s.minTileResolution, out.atlasSize),
                                            std::min(s.minTileResolution, out.atlasSize), rects);
        if (size == 0) return false;

        ShadowLightAssignment a;
        a.firstView = static_cast<int32_t>(out.views.size());
        a.viewCount = count;

        for (uint32_t i = 0; i < count; ++i)
        {
            const ShadowMath::PerspectiveView pv = c.isSpot
                ? ShadowMath::BuildSpotView(c.position, c.direction, c.outerRadians, c.range, size, pcf)
                : ShadowMath::BuildPointFace(c.position, i, c.range, size, pcf);

            ShadowView view;
            view.kind = c.isSpot ? ShadowView::Kind::Spot : ShadowView::Kind::PointFace;
            view.lightId = c.id;
            view.viewProj = pv.viewProj;
            view.x = rects[i].x; view.y = rects[i].y; view.size = rects[i].size;
            out.views.push_back(std::move(view));

            // perspective depth is not linear: a small fixed NDC bias (scaled by the setting) + the normal offset do the work
            out.gpuViews.push_back(ShadowMath::MakeGpuView(pv.viewProj, rects[i], out.atlasSize, s.normalBias, pv.texelWorldAtUnit,
                                                           s.constantBias * 1e-4f, true));
            CullVolume vol;
            vol.isBox = false;
            vol.frustum = pv.frustum;
            m_Volumes.push_back(vol);
        }
        out.lights[c.id] = a;
        return true;
    }
}
