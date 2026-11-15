#include "dxpch.h"
#include "DepthPrepass.h"
#include <algorithm>
#include "../Cache/ResourceCache.h"
#include "RHI/GraphicsDevice.h"
#include "utils/material/Material.h"

namespace DXEngine
{
    void DepthPrepass::Execute(FrameContext& ctx)
    {
        if (!ctx.environment.depthPrepass || !ctx.cmd || !ctx.targets.color || !ctx.targets.depth)
            return;

        // Wireframe draws only the edges: a solid depth fill would hide the edges of everything behind the front surface,
        // and the wireframe view is for seeing all of them.
        if (ctx.services->resources->GetWireframeMode())
            return;

        // nothing to do (no opaque item takes part) -> no state changes either
        std::vector<DrawItem>& items = ctx.batches.opaque;
        const bool any = std::any_of(items.begin(), items.end(), [](const DrawItem& item)
        {
            return item.mesh && item.material && UsesDepthPrepass(item.material->GetRenderState());
        });
        if (!any)
            return;

        ctx.cmd->SetRenderTarget(nullptr, ctx.targets.depth);         // depth only: no colour target

        RHI::ViewportDesc vp;
        vp.width    = static_cast<float>(ctx.targets.color->GetWidth());
        vp.height   = static_cast<float>(ctx.targets.color->GetHeight());
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        ctx.cmd->SetViewport(vp);

        m_Drawer.BeginPrepass(ctx);
        for (DrawItem& item : items)
        {
            if (!item.material || !UsesDepthPrepass(item.material->GetRenderState()))
                continue;
            item.depthPrepassed = m_Drawer.DrawPrepass(ctx, item);
        }
    }
}
