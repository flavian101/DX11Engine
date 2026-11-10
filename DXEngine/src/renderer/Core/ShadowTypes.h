#pragma once
#include "RenderTypes.h"
#include "utils/ConstantBufferTypes.h"
#include <DirectXMath.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace DXEngine
{
    namespace RHI { class ITexture; class IBuffer; }

    constexpr uint32_t kMaxShadowCascades = 4;

    // ------------------------------------------------------------------
    // ONE depth render of the shadow pass: a rectangle (tile) of the atlas plus the casters that touch it.
    // ------------------------------------------------------------------
    struct ShadowView
    {
        enum class Kind : uint8_t { Cascade, Spot, PointFace };

        Kind                kind = Kind::Cascade;
        uint32_t            lightId = 0;                    // entt::to_integral of the light's entity
        DirectX::XMFLOAT4X4 viewProj;                       // world -> light clip, NOT transposed
        uint32_t            x = 0, y = 0, size = 0;         // the tile, in atlas pixels (square)
        std::vector<uint32_t> casters;                      // indices into ShadowFrame::casters
    };

    // Where a light's shadow views are, so LightPacking can write them into the light buffers.
    struct ShadowLightAssignment
    {
        int32_t  firstView = -1;                            // index into ShadowFrame::views / gpuViews
        uint32_t viewCount = 0;                             // directional: cascades. spot: 1. point: 6
        DirectX::XMFLOAT4 cascadeSplits{ 0, 0, 0, 0 };      // directional only: far edge of each cascade (view depth)
        float    fadeStart = 0.0f;                          // directional only: depth where the last cascade starts fading
    };
    using ShadowLightTable = std::unordered_map<uint32_t, ShadowLightAssignment>;   // key: entity id

    // ==================================================================
    // ShadowFrame: everything the shadow pass and the lit shaders need, for ONE frame.
    //   ShadowPlanner (CPU, pure)  fills casters / views / gpuViews / lights
    //   ShadowSystem  (uploads)    fills the three GPU pointers
    //   ShadowPass    (GPU)        renders `views` into `atlas`
    //   MeshDrawer                 binds atlas + viewBuffer + constantBuffer for every lit draw
    // The top-level vectors keep their capacity from frame to frame (the per-view caster lists are small and rebuilt).
    // ==================================================================
    struct ShadowFrame
    {
        std::vector<DrawItem>      casters;      // each opaque shadow caster ONCE, however many views see it
        std::vector<ShadowView>    views;
        std::vector<ShadowViewGPU> gpuViews;     // views[i] <-> gpuViews[i]
        ShadowLightTable           lights;
        uint32_t                   atlasSize = 0;

        RHI::ITexture* atlas          = nullptr; // null when nothing was planned (shaders never read it then)
        RHI::IBuffer*  viewBuffer     = nullptr; // StructuredBuffer<ShadowView>, SRV_ShadowViews
        RHI::IBuffer*  constantBuffer = nullptr; // ShadowConstants, b5

        bool HasWork() const { return atlas && !views.empty(); }

        void Clear()
        {
            casters.clear();
            views.clear();
            gpuViews.clear();
            lights.clear();
            atlasSize = 0;
            atlas = nullptr; viewBuffer = nullptr; constantBuffer = nullptr;
        }
    };
}
