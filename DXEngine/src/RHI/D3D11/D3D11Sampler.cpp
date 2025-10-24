#include "dxpch.h"
#include "D3D11Sampler.h"

namespace DXEngine::RHI
{
    D3D11Sampler::D3D11Sampler(ID3D11Device* device, const SamplerDesc& desc)
        : m_Desc(desc)
        , m_DebugName(desc.debugName)
    {
        if (!device)
        {
            throw std::runtime_error("D3D11Sampler: Device cannot be null");
        }

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = GetD3DFilter(desc);
        samplerDesc.AddressU = GetD3DAddressMode(desc.addressU);
        samplerDesc.AddressV = GetD3DAddressMode(desc.addressV);
        samplerDesc.AddressW = GetD3DAddressMode(desc.addressW);
        samplerDesc.MipLODBias = desc.mipLODBias;
        samplerDesc.MaxAnisotropy = desc.maxAnisotropy;
        samplerDesc.ComparisonFunc = GetD3DComparisonFunc(desc.comparisonFunc);
        samplerDesc.BorderColor[0] = desc.borderColor[0];
        samplerDesc.BorderColor[1] = desc.borderColor[1];
        samplerDesc.BorderColor[2] = desc.borderColor[2];
        samplerDesc.BorderColor[3] = desc.borderColor[3];
        samplerDesc.MinLOD = desc.minLOD;
        samplerDesc.MaxLOD = desc.maxLOD;

        HRESULT hr = device->CreateSamplerState(&samplerDesc, m_Sampler.GetAddressOf());
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to create D3D11 sampler state");
        }

        if (!m_DebugName.empty())
        {
            SetDebugName(m_DebugName);
        }
    }

    void D3D11Sampler::SetDebugName(const std::string& name)
    {
        m_DebugName = name;
        if (m_Sampler)
        {
            m_Sampler->SetPrivateData(WKPDID_D3DDebugObjectName,
                static_cast<UINT>(name.length()), name.c_str());
        }
    }

    D3D11_FILTER D3D11Sampler::GetD3DFilter(const SamplerDesc& desc)
    {
        // Build filter from min, mag, mip filters
        if (desc.minFilter == FilterMode::Anisotropic ||
            desc.magFilter == FilterMode::Anisotropic)
        {
            return desc.enableComparison ?
                D3D11_FILTER_COMPARISON_ANISOTROPIC :
                D3D11_FILTER_ANISOTROPIC;
        }

        bool minPoint = (desc.minFilter == FilterMode::Point);
        bool magPoint = (desc.magFilter == FilterMode::Point);
        bool mipPoint = (desc.mipFilter == FilterMode::Point);

        if (desc.enableComparison)
        {
            if (minPoint && magPoint && mipPoint)
                return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
            if (minPoint && magPoint && !mipPoint)
                return D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
            if (minPoint && !magPoint && mipPoint)
                return D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
            if (minPoint && !magPoint && !mipPoint)
                return D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
            if (!minPoint && magPoint && mipPoint)
                return D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
            if (!minPoint && magPoint && !mipPoint)
                return D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
            if (!minPoint && !magPoint && mipPoint)
                return D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
            return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        }
        else
        {
            if (minPoint && magPoint && mipPoint)
                return D3D11_FILTER_MIN_MAG_MIP_POINT;
            if (minPoint && magPoint && !mipPoint)
                return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            if (minPoint && !magPoint && mipPoint)
                return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
            if (minPoint && !magPoint && !mipPoint)
                return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
            if (!minPoint && magPoint && mipPoint)
                return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
            if (!minPoint && magPoint && !mipPoint)
                return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
            if (!minPoint && !magPoint && mipPoint)
                return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        }
    }

    D3D11_TEXTURE_ADDRESS_MODE D3D11Sampler::GetD3DAddressMode(AddressMode mode)
    {
        switch (mode)
        {
        case AddressMode::Wrap:   return D3D11_TEXTURE_ADDRESS_WRAP;
        case AddressMode::Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
        case AddressMode::Clamp:  return D3D11_TEXTURE_ADDRESS_CLAMP;
        case AddressMode::Border: return D3D11_TEXTURE_ADDRESS_BORDER;
        default:                  return D3D11_TEXTURE_ADDRESS_WRAP;
        }
    }

    D3D11_COMPARISON_FUNC D3D11Sampler::GetD3DComparisonFunc(ComparisonFunc func)
    {
        switch (func)
        {
        case ComparisonFunc::Never:        return D3D11_COMPARISON_NEVER;
        case ComparisonFunc::Less:         return D3D11_COMPARISON_LESS;
        case ComparisonFunc::Equal:        return D3D11_COMPARISON_EQUAL;
        case ComparisonFunc::LessEqual:    return D3D11_COMPARISON_LESS_EQUAL;
        case ComparisonFunc::Greater:      return D3D11_COMPARISON_GREATER;
        case ComparisonFunc::NotEqual:     return D3D11_COMPARISON_NOT_EQUAL;
        case ComparisonFunc::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
        case ComparisonFunc::Always:       return D3D11_COMPARISON_ALWAYS;
        default:                           return D3D11_COMPARISON_NEVER;
        }
    }

}