#pragma once
// ============================================================================
// GpuTimings: the bookkeeping behind the GPU profiler, with no GPU in it (tested natively: tests/gpu_timings_tests.cpp).
//
// A GPU reports a timestamp query a few frames AFTER it was recorded, so a profiler keeps a ring of frame slots. Each slot
// remembers the NAMES of the scopes recorded into it (two timestamps per scope: begin, end). When the slot's query result
// arrives, Resolve() turns raw ticks into milliseconds, and keeps a running average so a number can be read and compared.
// ============================================================================
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace DXEngine
{
    struct GpuScopeTiming
    {
        std::string name;
        float ms    = 0.0f;       // the latest resolved frame
        float avgMs = 0.0f;       // running average: steady enough to read, and to compare two settings
    };

    struct GpuFrameTimings
    {
        std::vector<GpuScopeTiming> scopes;
        float    totalMs = 0.0f;  // first scope's start to last scope's end (gaps between scopes included)
        float    avgTotalMs = 0.0f;
        bool     valid = false;   // at least one frame has been resolved
        uint32_t droppedScopes = 0;   // scopes of the latest frame that did not fit (see kMaxScopes)
    };

    class GpuTimingCollector
    {
    public:
        static constexpr uint32_t kMaxScopes = 32;       // per frame
        static constexpr float    kAverageWeight = 0.1f; // share of the newest frame in the running average

        explicit GpuTimingCollector(uint32_t slots = 3) : m_Slots(std::max(1u, slots)) {}

        uint32_t SlotCount() const { return static_cast<uint32_t>(m_Slots.size()); }
        bool     IsPending(uint32_t slot) const { return slot < m_Slots.size() && m_Slots[slot].pending; }
        uint32_t ScopeCount(uint32_t slot) const { return slot < m_Slots.size() ? static_cast<uint32_t>(m_Slots[slot].names.size()) : 0; }

        // ---- recording
        void BeginRecording(uint32_t slot)
        {
            if (slot >= m_Slots.size()) return;
            Slot& s = m_Slots[slot];
            s.names.clear();
            s.dropped = 0;
            s.pending = false;
        }

        // The index of the new scope (its timestamps are slots 2*i and 2*i+1), or -1 when the frame is full.
        int AddScope(uint32_t slot, const std::string& name)
        {
            if (slot >= m_Slots.size()) return -1;
            Slot& s = m_Slots[slot];
            if (s.names.size() >= kMaxScopes) { ++s.dropped; return -1; }
            s.names.push_back(name);
            return static_cast<int>(s.names.size()) - 1;
        }

        void EndRecording(uint32_t slot) { if (slot < m_Slots.size()) m_Slots[slot].pending = true; }

        // ---- results
        // 'data' is the layout of IQuery::GetData for a timestamp query: [0] ticks per second (0 = discard), then two ticks
        // per scope. 'count' = how many uint64 values 'data' holds.
        void Resolve(uint32_t slot, const uint64_t* data, size_t count)
        {
            if (slot >= m_Slots.size()) return;
            Slot& s = m_Slots[slot];
            s.pending = false;

            const size_t scopes = s.names.size();
            if (!data || count < 1 + 2 * scopes || data[0] == 0 || scopes == 0)
                return;                                   // disjoint or empty: keep what we have

            const double toMs = 1000.0 / static_cast<double>(data[0]);
            std::vector<float> ms(scopes);
            uint64_t first = UINT64_MAX, last = 0;
            for (size_t i = 0; i < scopes; ++i)
            {
                const uint64_t begin = data[1 + 2 * i], end = data[2 + 2 * i];
                ms[i] = end >= begin ? static_cast<float>(static_cast<double>(end - begin) * toMs) : 0.0f;
                first = std::min(first, begin);
                last  = std::max(last, end);
            }
            const float total = last >= first ? static_cast<float>(static_cast<double>(last - first) * toMs) : 0.0f;

            // the same scopes as before -> keep averaging; a different set (a pass was added or removed) starts over
            bool sameScopes = m_Timings.valid && m_Timings.scopes.size() == scopes;
            for (size_t i = 0; sameScopes && i < scopes; ++i)
                sameScopes = m_Timings.scopes[i].name == s.names[i];

            if (!sameScopes)
            {
                m_Timings.scopes.assign(scopes, GpuScopeTiming{});
                for (size_t i = 0; i < scopes; ++i)
                    m_Timings.scopes[i] = { s.names[i], ms[i], ms[i] };
                m_Timings.totalMs = m_Timings.avgTotalMs = total;
            }
            else
            {
                for (size_t i = 0; i < scopes; ++i)
                {
                    m_Timings.scopes[i].ms = ms[i];
                    m_Timings.scopes[i].avgMs += (ms[i] - m_Timings.scopes[i].avgMs) * kAverageWeight;
                }
                m_Timings.totalMs = total;
                m_Timings.avgTotalMs += (total - m_Timings.avgTotalMs) * kAverageWeight;
            }
            m_Timings.valid = true;
            m_Timings.droppedScopes = s.dropped;
        }

        // The slot's result did not arrive in time and the slot is needed again: forget it, keep the old numbers.
        void Abandon(uint32_t slot) { if (slot < m_Slots.size()) m_Slots[slot].pending = false; }

        // Forget everything (profiling was switched off: stale numbers would mislead).
        void Reset()
        {
            for (Slot& s : m_Slots) { s.names.clear(); s.pending = false; s.dropped = 0; }
            m_Timings = GpuFrameTimings{};
        }

        const GpuFrameTimings& Timings() const { return m_Timings; }

    private:
        struct Slot
        {
            std::vector<std::string> names;
            uint32_t dropped = 0;
            bool     pending = false;      // recorded, result not read yet
        };
        std::vector<Slot> m_Slots;
        GpuFrameTimings   m_Timings;
    };
}
