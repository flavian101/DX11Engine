#pragma once
#include "RHI/GraphicsDevice.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace DXEngine::RHI
{
	using Microsoft::WRL::ComPtr;

    class D3D11Texture : public ITexture {
    public:
        D3D11Texture(ID3D11Device* device, const TextureDesc& desc);
        ~D3D11Texture() override = default;

        // IGraphicsResource
        const std::string& GetDebugName() const override { return m_DebugName; }
        void SetDebugName(const std::string& name) override;
        uint64_t GetGPUHandle() const override {
            return reinterpret_cast<uint64_t>(m_SRV.Get());
        }
        size_t GetMemoryUsage() const override;
        bool IsValid() const override;

        // ITexture
        TextureType GetType() const override { return m_Desc.type; }
        TextureFormat GetFormat() const override { return m_Desc.format; }
        uint32_t GetWidth() const override { return m_Desc.width; }
        uint32_t GetHeight() const override { return m_Desc.height; }
        uint32_t GetDepth() const override { return m_Desc.depth; }
        uint32_t GetMipLevels() const override { return m_Desc.mipLevels; }

        bool Update(const void* data, uint32_t mipLevel, uint32_t arraySlice) override;
        bool GenerateMips() override;
        void* GetNativeHandle() const override { return m_SRV.Get(); }

        // D3D11-specific
        ID3D11Texture2D* GetD3D11Texture2D() const { return m_Texture2D.Get(); }
        ID3D11Texture3D* GetD3D11Texture3D() const { return m_Texture3D.Get(); }
        ID3D11ShaderResourceView* GetSRV() const { return m_SRV.Get(); }
        ID3D11RenderTargetView* GetRTV() const { return m_RTV.Get(); }
        ID3D11DepthStencilView* GetDSV() const { return m_DSV.Get(); }

    private:
        bool CreateTexture2D(const TextureDesc& desc);
        bool CreateTexture3D(const TextureDesc& desc);
        bool CreateViews(const TextureDesc& desc, DXGI_FORMAT format);
        uint32_t CalculateMipSize(uint32_t baseDimension, uint32_t mipLevel) const;

        ComPtr<ID3D11Texture2D> m_Texture2D;
        ComPtr<ID3D11Texture3D> m_Texture3D;
        ComPtr<ID3D11ShaderResourceView> m_SRV;
        ComPtr<ID3D11RenderTargetView> m_RTV;
        ComPtr<ID3D11DepthStencilView> m_DSV;
        ComPtr<ID3D11Device> m_Device;
        ComPtr<ID3D11DeviceContext> m_Context;

        TextureDesc m_Desc;
        std::string m_DebugName;
    };

}