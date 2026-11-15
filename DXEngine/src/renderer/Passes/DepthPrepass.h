#pragma once
#include "../Core/FrameContext.h"
#include "../Core/RenderPass.h"
#include "MeshDrawer.h"

namespace DXEngine
{
    // ==================================================================
    // DepthPrepass: draws the OPAQUE geometry once, depth only, before anything is shaded.
    //
    // Why: the lit pixel shader loops over the lights of its cluster and samples shadows, so it is by far the most
    // expensive thing a pixel does. Without a pre-pass every covering triangle pays for it, front to back or not. With it
    // the colour pass (ForwardPass opaque) shades only the pixel that stayed in front: its depth test is LessEqual and it
    // no longer writes depth. The cost is a second, cheap vertex pass, which is why it is a setting
    // (RenderEnvironment::depthPrepass) and not an assumption.
    //
    // What takes part: DrawItems of the opaque queue whose material passes UsesDepthPrepass(). The pass marks every item
    // it drew (DrawItem::depthPrepassed); the colour pass trusts the depth buffer ONLY for marked items, so an item the
    // pre-pass could not draw still renders correctly (normal depth test, writes depth).
    //
    // Reads:  ctx.batches.opaque, ctx.camera, ctx.targets, ctx.environment.depthPrepass
    // Writes: the scene depth buffer (GPU), DrawItem::depthPrepassed, ctx.stats.depthPrepassDraws
    //
    // Runs AFTER ClearPass (it needs the cleared depth) and BEFORE every forward pass.
    // ==================================================================
    class DepthPrepass final : public RenderPass
    {
    public:
        bool Initialize(FrameContext& ctx) override { return m_Drawer.Initialize(ctx); }
        void Execute(FrameContext& ctx) override;
        std::string GetName() const override { return "DepthPrepass"; }

    private:
        MeshDrawer m_Drawer;
    };
}
