#include "dxpch.h"
#include "ShadowPass.h"
#include "RHI/GraphicsDevice.h"
#include "utils/ConstantBufferTypes.h"

namespace DXEngine
{
    void ShadowPass::Execute(FrameContext& ctx)
    {
        ShadowFrame& shadows = ctx.shadows;
        if (!ctx.cmd || !shadows.HasWork()) return;     // nothing planned: no state changes, no clear
        RHI::ICommandBuffer& cmd = *ctx.cmd;

        // The atlas was a shader resource in last frame's lit draws. D3D11 cannot have one texture bound as a shader
        // resource AND as a depth target: unbind the SRV explicitly instead of letting the runtime do it with a warning.
        cmd.SetTexture(nullptr, SRV_ShadowAtlas, RHI::ShaderStage::Pixel);

        cmd.SetRenderTarget(nullptr, shadows.atlas);                 // depth only: no colour target
        cmd.ClearDepthStencil(shadows.atlas, 1.0f, 0);               // 1.0 = "nothing in the way" = lit

        const ShadowSettings& s = ctx.environment.shadows;
        m_Drawer.BeginDepthPass(ctx, s.rasterDepthBias, s.rasterSlopeBias);

        for (const ShadowView& view : shadows.views)
        {
            if (view.casters.empty()) continue;                      // an empty tile stays 1.0: fully lit

            // One view = one tile of the atlas. The scissor keeps a primitive from bleeding into the neighbouring tile.
            RHI::ViewportDesc vp;
            vp.x = static_cast<float>(view.x);
            vp.y = static_cast<float>(view.y);
            vp.width  = static_cast<float>(view.size);
            vp.height = static_cast<float>(view.size);
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            cmd.SetViewport(vp);
            cmd.SetScissor(view.x, view.y, view.size, view.size);

            m_Drawer.SetDepthViewProj(view.viewProj);
            for (uint32_t index : view.casters)
                m_Drawer.DrawDepth(ctx, shadows.casters[index]);
        }

        // Leave the pipeline state the way the next pass expects it: back buffer + scene depth, full-size viewport.
        // (ForwardPass sets these itself; this is for whatever comes next.)
        if (ctx.targets.backBuffer)
        {
            cmd.SetRenderTarget(ctx.targets.backBuffer, ctx.targets.depthBuffer);
            RHI::ViewportDesc full;
            full.width  = static_cast<float>(ctx.targets.backBuffer->GetWidth());
            full.height = static_cast<float>(ctx.targets.backBuffer->GetHeight());
            cmd.SetViewport(full);
        }
    }
}
