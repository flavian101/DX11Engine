#pragma once
#include "RHI/GraphicsDevice.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace DXEngine::RHI
{
    using Microsoft::WRL::ComPtr;

    class D3D11Sampler : public ISampler
    {
    public:
        D3D11Sampler(ID3D11Device* device, const SamplerDesc& desc);
        ~D3D11Sampler() override = default;

        // IGraphicsResource
        const std::string& GetDebugName() const override { return m_DebugName; }
        void SetDebugName(const std::string& name) override;
        uint64_t GetGPUHandle() const override {
            return reinterpret_cast<uint64_t>(m_Sampler.Get());
        }
        size_t GetMemoryUsage() const override { return sizeof(*this); }
        bool IsValid() const override { return m_Sampler != nullptr; }

        // ISampler
        const SamplerDesc& GetDesc() const override { return m_Desc; }
        void* GetNativeHandle() const override { return m_Sampler.Get(); }

        // D3D11-specific
        ID3D11SamplerState* GetD3D11Sampler() const { return m_Sampler.Get(); }

    private:
        ComPtr<ID3D11SamplerState> m_Sampler;
        SamplerDesc m_Desc;
        std::string m_DebugName;

        D3D11_FILTER GetD3DFilter(const SamplerDesc& desc);
        D3D11_TEXTURE_ADDRESS_MODE GetD3DAddressMode(AddressMode mode);
        D3D11_COMPARISON_FUNC GetD3DComparisonFunc(ComparisonFunc func);
    };

} 


