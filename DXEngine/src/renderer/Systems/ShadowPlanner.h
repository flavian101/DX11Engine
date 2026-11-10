#pragma once
#include "../Core/ShadowTypes.h"
#include "SceneExtraction.h"
#include "ShadowMath.h"
#include <DirectXCollision.h>
#include <vector>

namespace DXEngine
{
    class Scene;

    // ==================================================================
    // ShadowPlanner: decides WHAT gets shadow-mapped this frame. Pure CPU (no GPU calls), so it is unit-tested.
    //
    //   1. directional lights  : the strongest ones (maxShadowedDirectional) get cascadeCount cascades each,
    //                            fitted to slices of the camera frustum (ShadowMath::FitCascade)
    //   2. point / spot lights : only lights whose reach touches the camera frustum are candidates; the most
    //                            IMPORTANT ones (big on screen = range / distance) get views, up to maxShadowedLocalLights.
    //                            Closer / bigger lights get bigger tiles; far ones get smaller tiles.
    //   3. the atlas           : every view needs a tile. A light gets ALL its tiles or none (a point light with five of
    //                            six faces would have a hole in its shadow). When they do not fit, the tiles shrink (down
    //                            to minTileResolution), then the light is dropped and counted in shadowLightsDropped.
    //   4. casters             : the opaque casters are extracted ONCE (SceneExtraction::ExtractShadowCasters), then each
    //                            view keeps the indices of the ones that can reach it.
    //
    // Output: ShadowFrame::{casters, views, gpuViews, lights, atlasSize} and the shadow counters in `stats`.
    // The caller (ShadowSystem) uploads gpuViews and sets the GPU pointers.
    // ==================================================================
    class ShadowPlanner
    {
    public:
        void Plan(Scene* scene, const ExtractionCamera& camera, const ShadowSettings& settings,
                  const SceneExtraction& extraction, ShadowFrame& out, RenderStatistics& stats);

    private:
        struct CullVolume
        {
            bool                         isBox = false;
            DirectX::BoundingOrientedBox box;
            DirectX::BoundingFrustum     frustum;
            bool Reaches(const DirectX::BoundingSphere& s) const
            {
                return isBox ? box.Intersects(s) : frustum.Intersects(s);
            }
        };

        struct LocalCandidate
        {
            uint32_t          id = 0;
            bool              isSpot = false;
            DirectX::XMFLOAT3 position{ 0, 0, 0 };
            DirectX::XMFLOAT3 direction{ 0, 0, 1 };
            float             range = 0.0f;
            float             outerRadians = 0.0f;
            float             importance = 0.0f;
        };

        bool PlanDirectional(uint32_t id, const DirectX::XMFLOAT3& direction, const ExtractionCamera& camera,
                             const ShadowSettings& s, ShadowFrame& out);
        bool PlanLocal(const LocalCandidate& c, const ShadowSettings& s, ShadowFrame& out);

        // allocates `count` tiles of `size` (shrinking by half down to the minimum) all-or-nothing; returns the size or 0
        uint32_t AllocateTiles(uint32_t count, uint32_t size, uint32_t minSize, ShadowMath::AtlasRect* rects);

        ShadowMath::AtlasAllocator              m_Atlas;
        std::vector<CullVolume>                 m_Volumes;      // parallel to ShadowFrame::views
        std::vector<DirectX::BoundingSphere>    m_CasterBounds;
        std::vector<LocalCandidate>             m_Candidates;
    };
}
