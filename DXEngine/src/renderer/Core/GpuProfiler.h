#pragma once
#include "GpuTimings.h"
#include "RHI/GraphicsDevice.h"
#include <memory>
#include <string>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // GpuProfiler: how long the GPU spent in each named scope (the render graph wraps every pass in one).
    //
    // One timestamp query per frame in flight (kFramesInFlight). The result of a frame is read when its slot comes round
    // again, so nothing ever waits for the GPU; the numbers lag by a few frames, which a profiler can live with.
    // A result that is still not ready when its slot is needed is dropped and the previous numbers stay.
    //
    // Call order per frame: BeginFrame, then BeginScope/EndScope pairs (one open scope at a time), then EndFrame.
    // ==================================================================
    class GpuProfiler
    {
    public:
        static constexpr uint32_t kFramesInFlight = 3;

        // False when the device cannot create the queries: the profiler then stays unavailable and does nothing.
        bool Initialize(const std::shared_ptr<RHI::IGraphicsDevice>& device);
        void Shutdown();

        void BeginFrame(RHI::ICommandBuffer& cmd);
        void BeginScope(RHI::ICommandBuffer& cmd, const std::string& name);
        void EndScope(RHI::ICommandBuffer& cmd);
        void EndFrame(RHI::ICommandBuffer& cmd);

        // Takes effect at the next BeginFrame (never in the middle of a frame). Switching off forgets the old numbers.
        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_Enabled; }
        bool IsAvailable() const { return !m_Queries.empty(); }

        const GpuFrameTimings& GetTimings() const { return m_Collector.Timings(); }

    private:
        std::vector<std::shared_ptr<RHI::IQuery>> m_Queries;     // one per slot
        GpuTimingCollector m_Collector{ kFramesInFlight };
        uint32_t m_FrameCounter = 0;
        uint32_t m_Slot = 0;               // the slot being recorded
        bool     m_InFrame = false;
        int      m_OpenScope = -1;
        bool     m_Enabled = true;
        bool     m_WasEnabled = true;
    };
}
