#pragma once
#include "../Core/FrameContext.h"
#include "../Core/RenderPass.h"
#include "RHI/GraphicsDevice.h"

namespace DXEngine
{
    // Clears colour + depth. First pass in the graph.
    // Reads:  ctx.targets.color / depth, ctx.clearColor (the view's).     Writes: the GPU targets (cleared).
    class ClearPass final : public RenderPass
    {
    public:
        struct Config
        {
            float   depth         = 1.0f;
            uint8_t stencil       = 0;
            bool    clearColor    = true;     // (old struct had TWO members named clearColor: did not compile)
            bool    clearDepth    = true;
        };

        // Two constructors instead of '= {}': a nested struct's default member initializers cannot be
        // used in a default argument of the enclosing class (GCC/Clang reject it).
        ClearPass() = default;
        explicit ClearPass(const Config& cfg) : m_Config(cfg) {}

        void Execute(FrameContext& ctx) override
        {
            RHI::ICommandBuffer* cmd = ctx.cmd;
            if (!cmd || !ctx.targets.color) return;

            if (m_Config.clearColor)
                cmd->ClearRenderTarget(ctx.targets.color,
                                       ctx.clearColor[0], ctx.clearColor[1], ctx.clearColor[2], ctx.clearColor[3]);

            if (m_Config.clearDepth && ctx.targets.depth)
                cmd->ClearDepthStencil(ctx.targets.depth, m_Config.depth, m_Config.stencil);
        }

        std::string GetName() const override { return "ClearPass"; }

    private:
        Config m_Config;
    };
}
