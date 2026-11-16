#pragma once
#include "RHI/GraphicsDevice.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <deque>
#include <vector>

namespace DXEngine::RHI
{
    using Microsoft::WRL::ComPtr;

    // ==================================================================
    // D3D11Query: IQuery on top of ID3D11Query. 'count' slots, one ID3D11Query each.
    //
    // A Timestamp query additionally owns the TIMESTAMP_DISJOINT query D3D11 needs around timestamps: BeginQuery opens it,
    // EndQuery closes it, WriteTimestamp(slot) is an End() on that slot's query. Slots that were not written since the
    // last BeginQuery are not waited for (their query was never issued and would never be ready) and read back as 0.
    // ==================================================================
    class D3D11Query final : public IQuery
    {
    public:
        D3D11Query(ID3D11Device* device, ID3D11DeviceContext* context, const QueryDesc& desc);

        // IGraphicsResource
        const std::string& GetDebugName() const override { return m_DebugName; }
        void SetDebugName(const std::string& name) override { m_DebugName = name; }
        uint64_t GetGPUHandle() const override { return reinterpret_cast<uint64_t>(m_Slots.empty() ? nullptr : m_Slots[0].Get()); }
        size_t GetMemoryUsage() const override { return sizeof(*this); }
        bool IsValid() const override { return m_Valid; }

        // IQuery
        QueryType GetType() const override { return m_Type; }
        bool GetData(void* outData, size_t dataSize) override;
        bool IsReady() const override;

        // recording (called by D3D11CommandBuffer)
        void Begin(uint32_t slot);
        void End(uint32_t slot);
        void WriteTimestamp(uint32_t slot);

    private:
        bool SlotReady(uint32_t slot) const;

        ID3D11DeviceContext*             m_Context = nullptr;    // the immediate context (outlives every query)
        QueryType                        m_Type;
        std::vector<ComPtr<ID3D11Query>> m_Slots;
        std::vector<bool>                m_Issued;               // slot was ended since the last Begin
        ComPtr<ID3D11Query>              m_Disjoint;             // Timestamp only
        bool                             m_DisjointIssued = false;
        bool                             m_Valid = false;
        std::string                      m_DebugName;
    };

    // ==================================================================
    // D3D11Fence: D3D11 has no fences; an EVENT query is the equivalent. Signal(value) issues one, and the fence has
    // reached 'value' once that event is done (everything submitted before it has finished). Events finish in order, so
    // checking the oldest first is enough.
    // ==================================================================
    class D3D11Fence final : public IFence
    {
    public:
        D3D11Fence(ID3D11Device* device, ID3D11DeviceContext* context, const FenceDesc& desc);

        // IGraphicsResource
        const std::string& GetDebugName() const override { return m_DebugName; }
        void SetDebugName(const std::string& name) override { m_DebugName = name; }
        uint64_t GetGPUHandle() const override { return 0; }
        size_t GetMemoryUsage() const override { return sizeof(*this) + m_Pending.size() * sizeof(Pending); }
        bool IsValid() const override { return m_Device != nullptr && m_Context != nullptr; }

        // IFence
        void Signal(uint64_t value) override;
        void Wait(uint64_t value, uint64_t timeout = UINT64_MAX) override;
        uint64_t GetCompletedValue() const override;
        uint64_t GetLastSignaledValue() const override { return m_LastSignaled; }

    private:
        struct Pending { uint64_t value; ComPtr<ID3D11Query> event; };

        void Poll() const;                                  // retire every event the GPU has passed

        ComPtr<ID3D11Device>        m_Device;
        ComPtr<ID3D11DeviceContext> m_Context;
        mutable std::deque<Pending> m_Pending;              // oldest first
        mutable uint64_t            m_Completed = 0;
        uint64_t                    m_LastSignaled = 0;
        std::string                 m_DebugName;
    };
}
