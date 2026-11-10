#pragma once
#include "../Core/FrameContext.h"
#include "../Core/RenderPass.h"
#include "MeshDrawer.h"

namespace DXEngine
{
    // Renders every planned shadow view into its tile of the shadow atlas: depth only, no pixel shader, no colour target.
    //
    // Reads:  ctx.shadows (views, casters, atlas), ctx.environment.shadows (rasterizer bias)
    // Writes: the atlas (GPU), ctx.stats.shadowCasterDraws
    //
    // Runs FIRST in the graph: the atlas is sampled by every lit draw that follows.
    class ShadowPass final : public RenderPass
    {
    public:
        bool Initialize(FrameContext& ctx) override { return m_Drawer.Initialize(ctx); }
        void Execute(FrameContext& ctx) override;
        std::string GetName() const override { return "ShadowPass"; }

    private:
        MeshDrawer m_Drawer;
    };
}
