#pragma once
#include "../Core/FrameContext.h"
#include "../Core/RenderPass.h"
#include "RHI/GraphicsDevice.h"

namespace DXEngine
{
    // Clears colour + depth. First pass in the graph.
    // Reads:  ctx.targets.backBuffer / depthBuffer.     Writes: the GPU targets (cleared).
    class ClearPass final : public RenderPass
    {
    public:
        struct Config
        {
            float   color[4]      = { 0.1f, 0.1f, 0.1f, 1.0f };
            float   depth         = 1.0f;
            uint8_t stencil       = 0;
            bool    clearColor    = true;     // (old struct had TWO members named clearColor: did not compile)
            bool    clearDepth    = true;
        };

        explicit ClearPass(const Config& cfg = {}) : m_Config(cfg) {}

        void Execute(FrameContext& ctx) override
        {
            RHI::ICommandBuffer* cmd = ctx.cmd;
            if (!cmd || !ctx.targets.backBuffer) return;

            if (m_Config.clearColor)
                cmd->ClearRenderTarget(ctx.targets.backBuffer,
                                       m_Config.color[0], m_Config.color[1], m_Config.color[2], m_Config.color[3]);

            if (m_Config.clearDepth && ctx.targets.depthBuffer)
                cmd->ClearDepthStencil(ctx.targets.depthBuffer, m_Config.depth, m_Config.stencil);
        }

        std::string GetName() const override { return "ClearPass"; }

        void SetColor(float r, float g, float b, float a = 1.0f)
        {
            m_Config.color[0] = r; m_Config.color[1] = g; m_Config.color[2] = b; m_Config.color[3] = a;
        }

    private:
        Config m_Config;
    };
}
