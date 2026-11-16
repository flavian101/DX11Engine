#include "dxpch.h"
#include "D3D11Query.h"
#include <chrono>
#include <cstring>
#include <thread>

namespace DXEngine::RHI
{
    namespace
    {
        D3D11_QUERY ToD3D(QueryType type)
        {
            switch (type)
            {
            case QueryType::Occlusion:          return D3D11_QUERY_OCCLUSION;
            case QueryType::PipelineStatistics: return D3D11_QUERY_PIPELINE_STATISTICS;
            default:                            return D3D11_QUERY_TIMESTAMP;
            }
        }

        // 'S_OK' when the GPU is done. DONOTFLUSH: polling must not force a flush every call; the frame's Present does.
        bool Done(ID3D11DeviceContext* context, ID3D11Query* query, void* data, UINT size)
        {
            return context->GetData(query, data, size, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
        }
    }

    // ------------------------------------------------------------------ D3D11Query
    D3D11Query::D3D11Query(ID3D11Device* device, ID3D11DeviceContext* context, const QueryDesc& desc)
        : m_Context(context), m_Type(desc.type), m_DebugName(desc.debugName)
    {
        if (!device || !context || desc.count == 0)
            return;

        D3D11_QUERY_DESC qd = {};
        qd.Query = ToD3D(desc.type);
        m_Slots.resize(desc.count);
        for (auto& slot : m_Slots)
            if (FAILED(device->CreateQuery(&qd, slot.GetAddressOf())))
                return;                                     // m_Valid stays false

        if (desc.type == QueryType::Timestamp)
        {
            D3D11_QUERY_DESC dd = {};
            dd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            if (FAILED(device->CreateQuery(&dd, m_Disjoint.GetAddressOf())))
                return;
        }
        m_Issued.assign(desc.count, false);
        m_Valid = true;
    }

    void D3D11Query::Begin(uint32_t slot)
    {
        if (!m_Valid) return;
        if (m_Type == QueryType::Timestamp)
        {
            m_Context->Begin(m_Disjoint.Get());             // the interval the timestamps are written in
            m_DisjointIssued = false;
            std::fill(m_Issued.begin(), m_Issued.end(), false);
        }
        else if (slot < m_Slots.size())
        {
            m_Context->Begin(m_Slots[slot].Get());
        }
    }

    void D3D11Query::End(uint32_t slot)
    {
        if (!m_Valid) return;
        if (m_Type == QueryType::Timestamp)
        {
            m_Context->End(m_Disjoint.Get());
            m_DisjointIssued = true;
        }
        else if (slot < m_Slots.size())
        {
            m_Context->End(m_Slots[slot].Get());
            m_Issued[slot] = true;
        }
    }

    void D3D11Query::WriteTimestamp(uint32_t slot)
    {
        if (!m_Valid || m_Type != QueryType::Timestamp || slot >= m_Slots.size()) return;
        m_Context->End(m_Slots[slot].Get());
        m_Issued[slot] = true;
    }

    bool D3D11Query::SlotReady(uint32_t slot) const
    {
        if (!m_Issued[slot]) return true;                   // never written: nothing to wait for
        switch (m_Type)
        {
        case QueryType::Occlusion:          { UINT64 v; return Done(m_Context, m_Slots[slot].Get(), &v, sizeof(v)); }
        case QueryType::PipelineStatistics: { D3D11_QUERY_DATA_PIPELINE_STATISTICS v; return Done(m_Context, m_Slots[slot].Get(), &v, sizeof(v)); }
        default:                            { UINT64 v; return Done(m_Context, m_Slots[slot].Get(), &v, sizeof(v)); }
        }
    }

    bool D3D11Query::IsReady() const
    {
        if (!m_Valid) return false;
        if (m_Type == QueryType::Timestamp)
        {
            if (!m_DisjointIssued) return false;            // the interval was never closed
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
            if (!Done(m_Context, m_Disjoint.Get(), &disjoint, sizeof(disjoint))) return false;
        }
        for (uint32_t i = 0; i < m_Slots.size(); ++i)
            if (!SlotReady(i)) return false;
        return true;
    }

    bool D3D11Query::GetData(void* outData, size_t dataSize)
    {
        if (!outData || !IsReady()) return false;

        const size_t count = m_Slots.size();
        switch (m_Type)
        {
        case QueryType::Timestamp:
        {
            if (dataSize < (count + 1) * sizeof(uint64_t)) return false;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
            if (!Done(m_Context, m_Disjoint.Get(), &disjoint, sizeof(disjoint))) return false;

            uint64_t* out = static_cast<uint64_t*>(outData);
            out[0] = disjoint.Disjoint ? 0 : disjoint.Frequency;
            for (size_t i = 0; i < count; ++i)
            {
                UINT64 ticks = 0;
                if (m_Issued[i] && !Done(m_Context, m_Slots[i].Get(), &ticks, sizeof(ticks))) return false;
                out[1 + i] = m_Issued[i] ? ticks : 0;
            }
            return true;
        }
        case QueryType::Occlusion:
        {
            if (dataSize < count * sizeof(uint64_t)) return false;
            uint64_t* out = static_cast<uint64_t*>(outData);
            for (size_t i = 0; i < count; ++i)
            {
                UINT64 samples = 0;
                if (m_Issued[i] && !Done(m_Context, m_Slots[i].Get(), &samples, sizeof(samples))) return false;
                out[i] = samples;
            }
            return true;
        }
        default:                                            // PipelineStatistics
        {
            if (dataSize < count * sizeof(PipelineStatisticsData)) return false;
            PipelineStatisticsData* out = static_cast<PipelineStatisticsData*>(outData);
            for (size_t i = 0; i < count; ++i)
            {
                D3D11_QUERY_DATA_PIPELINE_STATISTICS s = {};
                if (m_Issued[i] && !Done(m_Context, m_Slots[i].Get(), &s, sizeof(s))) return false;
                out[i] = { s.IAVertices, s.IAPrimitives, s.VSInvocations, s.GSInvocations, s.GSPrimitives,
                           s.CInvocations, s.CPrimitives, s.PSInvocations, s.HSInvocations, s.DSInvocations, s.CSInvocations };
            }
            return true;
        }
        }
    }

    // ------------------------------------------------------------------ D3D11Fence
    D3D11Fence::D3D11Fence(ID3D11Device* device, ID3D11DeviceContext* context, const FenceDesc& desc)
        : m_Device(device), m_Context(context), m_Completed(desc.initialValue), m_LastSignaled(desc.initialValue),
          m_DebugName(desc.debugName)
    {
    }

    void D3D11Fence::Signal(uint64_t value)
    {
        if (!IsValid() || value <= m_LastSignaled) return;  // values only go up

        D3D11_QUERY_DESC qd = {};
        qd.Query = D3D11_QUERY_EVENT;
        ComPtr<ID3D11Query> event;
        if (FAILED(m_Device->CreateQuery(&qd, event.GetAddressOf())))
        {
            OutputDebugStringA("D3D11Fence::Signal: could not create the event query\n");
            return;
        }
        m_Context->End(event.Get());
        m_Pending.push_back({ value, std::move(event) });
        m_LastSignaled = value;
    }

    void D3D11Fence::Poll() const
    {
        while (!m_Pending.empty())
        {
            BOOL done = FALSE;
            if (m_Context->GetData(m_Pending.front().event.Get(), &done, sizeof(done), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK || !done)
                return;                                     // the oldest is not done, so none of the later ones are
            m_Completed = m_Pending.front().value;
            m_Pending.pop_front();
        }
    }

    uint64_t D3D11Fence::GetCompletedValue() const
    {
        Poll();
        return m_Completed;
    }

    void D3D11Fence::Wait(uint64_t value, uint64_t timeout)
    {
        if (!IsValid() || value > m_LastSignaled) return;   // never signalled: nothing to wait for
        Poll();
        if (m_Completed >= value) return;

        m_Context->Flush();                                 // make sure what we wait for was actually sent to the GPU
        const auto start = std::chrono::steady_clock::now();
        while (GetCompletedValue() < value)
        {
            if (timeout != UINT64_MAX &&
                std::chrono::steady_clock::now() - start >= std::chrono::nanoseconds(timeout))
                return;
            std::this_thread::yield();
        }
    }
}
