#include "dxpch.h"
#include "GpuProfiler.h"

namespace DXEngine
{
    bool GpuProfiler::Initialize(const std::shared_ptr<RHI::IGraphicsDevice>& device)
    {
        Shutdown();
        if (!device) return false;

        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            RHI::QueryDesc desc;
            desc.type = RHI::QueryType::Timestamp;
            desc.count = 2 * GpuTimingCollector::kMaxScopes;               // begin + end per scope
            desc.debugName = "GpuProfiler frame " + std::to_string(i);
            std::shared_ptr<RHI::IQuery> query = device->CreateQuery(desc);
            if (!query || !query->IsValid())
            {
                m_Queries.clear();
                return false;
            }
            m_Queries.push_back(std::move(query));
        }
        return true;
    }

    void GpuProfiler::Shutdown()
    {
        m_Queries.clear();
        m_Collector.Reset();
        m_FrameCounter = m_Slot = 0;
        m_InFrame = false;
        m_OpenScope = -1;
    }

    void GpuProfiler::SetEnabled(bool enabled) { m_Enabled = enabled; }

    void GpuProfiler::BeginFrame(RHI::ICommandBuffer& cmd)
    {
        m_InFrame = false;
        m_OpenScope = -1;
        if (m_Queries.empty()) return;

        if (!m_Enabled)
        {
            if (m_WasEnabled) m_Collector.Reset();        // stale numbers would mislead once it is switched on again
            m_WasEnabled = false;
            return;
        }
        m_WasEnabled = true;

        m_Slot = m_FrameCounter++ % kFramesInFlight;
        RHI::IQuery& query = *m_Queries[m_Slot];

        // the slot's previous frame: read it if the GPU is done, drop it if not (never wait)
        if (m_Collector.IsPending(m_Slot))
        {
            uint64_t data[1 + 2 * GpuTimingCollector::kMaxScopes];
            if (query.IsReady() && query.GetData(data, sizeof(data)))
                m_Collector.Resolve(m_Slot, data, 1 + 2 * GpuTimingCollector::kMaxScopes);
            else
                m_Collector.Abandon(m_Slot);
        }

        m_Collector.BeginRecording(m_Slot);
        cmd.BeginQuery(&query, 0);
        m_InFrame = true;
    }

    void GpuProfiler::BeginScope(RHI::ICommandBuffer& cmd, const std::string& name)
    {
        if (!m_InFrame || m_OpenScope >= 0) return;
        const int index = m_Collector.AddScope(m_Slot, name);
        if (index < 0) return;                             // the frame is full: counted as dropped
        cmd.WriteTimestamp(m_Queries[m_Slot].get(), RHI::PipelineStage::Top, static_cast<uint32_t>(2 * index));
        m_OpenScope = index;
    }

    void GpuProfiler::EndScope(RHI::ICommandBuffer& cmd)
    {
        if (!m_InFrame || m_OpenScope < 0) return;
        cmd.WriteTimestamp(m_Queries[m_Slot].get(), RHI::PipelineStage::Bottom, static_cast<uint32_t>(2 * m_OpenScope + 1));
        m_OpenScope = -1;
    }

    void GpuProfiler::EndFrame(RHI::ICommandBuffer& cmd)
    {
        if (!m_InFrame) return;
        EndScope(cmd);
        cmd.EndQuery(m_Queries[m_Slot].get(), 0);
        m_Collector.EndRecording(m_Slot);
        m_InFrame = false;
    }
}
