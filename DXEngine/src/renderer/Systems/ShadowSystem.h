#pragma once
#include "../Core/FrameContext.h"
#include "ShadowPlanner.h"
#include <memory>

namespace DXEngine
{
    namespace RHI { class IBuffer; class ITexture; }

    // CPU system. Runs the ShadowPlanner (which lights get shadows, where their tiles are, which casters each view
    // draws) and uploads what the pixel shader reads:
    //   - ShadowConstants                       -> constant buffer b5   (always valid)
    //   - one ShadowViewGPU per view            -> structured buffer t36 (grows in doubling steps)
    // and makes sure the atlas (the depth texture the ShadowPass renders into and the lit shaders sample) exists.
    // All GPU objects are owned by the ResourceCache; this system only asks for them.
    //
    // Reads:  ctx.scene.world, ctx.camera (via the ExtractionCamera), ctx.environment.shadows
    // Writes: ctx.shadows (casters, views, gpuViews, lights, atlas, viewBuffer, constantBuffer), shadow counters in ctx.stats
    //
    // MUST run BEFORE LightSystem::Upload: the light buffers point at the views planned here.
    class ShadowSystem
    {
    public:
        bool Initialize(RenderServices& services);
        void Update(FrameContext& ctx, const SceneExtraction& extraction, const ExtractionCamera& camera);

    private:
        ShadowPlanner                 m_Planner;
        std::shared_ptr<RHI::IBuffer> m_Constants;        // b5
        std::shared_ptr<RHI::IBuffer> m_ViewBuffer;       // StructuredBuffer<ShadowViewGPU>
        uint32_t                      m_ViewCapacity = 0;
    };
}
