#pragma once
#include "../Core/RenderPass.h"
#include "MeshDrawer.h"

namespace DXEngine
{
    // ==================================================================
    // ForwardPass: draws ONE list from the BatchQueue into the back buffer.
    //   Queue::Background  -> skyboxes
    //   Queue::Opaque      -> solid geometry (depth write ON, set by the pipeline)
    //   Queue::Transparent -> blended geometry (depth write OFF, set by the pipeline)
    //
    // One class, three instances. The OLD design had separate Opaque / Transparent / Skybox
    // pass files that would all have contained the same bind-and-draw code.
    // Split into separate classes ONLY when they truly diverge (e.g. depth pre-pass).
    //
    // Reads:  ctx.batches.<queue>, ctx.camera, ctx.lightBuffer, ctx.localLightBuffer, ctx.targets
    // Writes: ctx.stats, the GPU back buffer / depth buffer
    // ==================================================================
    class ForwardPass final : public RenderPass
    {
    public:
        enum class Queue { Background, Opaque, Transparent };

        explicit ForwardPass(Queue queue) : m_Queue(queue) {}

        bool Initialize(FrameContext& ctx) override { return m_Drawer.Initialize(ctx); }
        void Execute(FrameContext& ctx) override;
        std::string GetName() const override;

    private:
        const std::vector<DrawItem>& Items(const FrameContext& ctx) const;

        Queue      m_Queue;
        MeshDrawer m_Drawer;
    };
}
