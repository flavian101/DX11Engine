#include "dxpch.h"
#include "ForwardPass.h"
#include "RHI/GraphicsDevice.h"

namespace DXEngine
{
    const std::vector<DrawItem>& ForwardPass::Items(const FrameContext& ctx) const
    {
        switch (m_Queue)
        {
        case Queue::Background:  return ctx.batches.background;
        case Queue::Transparent: return ctx.batches.transparent;
        default:                 return ctx.batches.opaque;
        }
    }

    std::string ForwardPass::GetName() const
    {
        switch (m_Queue)
        {
        case Queue::Background:  return "ForwardPass(background)";
        case Queue::Transparent: return "ForwardPass(transparent)";
        default:                 return "ForwardPass(opaque)";
        }
    }

    void ForwardPass::Execute(FrameContext& ctx)
    {
        const std::vector<DrawItem>& items = Items(ctx);
        if (items.empty() || !ctx.cmd || !ctx.targets.backBuffer) return;     // nothing to draw: no state changes either

        // WHERE we draw (the pass's job)
        ctx.cmd->SetRenderTarget(ctx.targets.backBuffer, ctx.targets.depthBuffer);

        RHI::ViewportDesc vp;
        vp.width    = static_cast<float>(ctx.targets.backBuffer->GetWidth());
        vp.height   = static_cast<float>(ctx.targets.backBuffer->GetHeight());
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        ctx.cmd->SetViewport(vp);

        // HOW we draw each item (the drawer's job)
        m_Drawer.BeginPass(ctx);
        for (const DrawItem& item : items)
            m_Drawer.Draw(ctx, item);
    }
}
