#include "dxpch.h"
#include "D3D11Texture.h"


namespace DXEngine::RHI
{
	static DXGI_FORMAT GetDXGIFormat(TextureFormat format) {
		switch (format) {
		case TextureFormat::RGBA8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat::RGBA16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case TextureFormat::RGBA32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case TextureFormat::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
		case TextureFormat::RG8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
		case TextureFormat::D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case TextureFormat::BC1_UNORM: return DXGI_FORMAT_BC1_UNORM;
		case TextureFormat::BC3_UNORM: return DXGI_FORMAT_BC3_UNORM;
		case TextureFormat::BC5_UNORM: return DXGI_FORMAT_BC5_UNORM;
		default: return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
	}

	static int GetBytesPerPixel(TextureFormat format) { //to be moved in the Interface class since it for all platforms
		switch (format) {
		case TextureFormat::RGBA8_UNORM:        return 4;
		case TextureFormat::RGBA16_FLOAT:       return 8;
		case TextureFormat::RGBA32_FLOAT:       return 16;
		case TextureFormat::R8_UNORM:           return 1;
		case TextureFormat::RG8_UNORM:          return 2;
		case TextureFormat::D24_UNORM_S8_UINT:  return 4;  // 3 bytes depth + 1 byte stencil
		case TextureFormat::BC1_UNORM:          return 8;  // Compressed: 8 bytes per 4x4 block
		case TextureFormat::BC3_UNORM:          return 16;  // Compressed: 16 bytes per 4x4 block
		case TextureFormat::BC5_UNORM:          return 16;  // Compressed: 16 bytes per 4x4 block
		default:                                return -1; // Unknown format
		}
	}

    D3D11Texture::D3D11Texture(ID3D11Device* device, const TextureDesc& desc)
        : m_Desc(desc)
        , m_DebugName(desc.debugName)
    {
        if (!device) {
            throw std::runtime_error("D3D11Texture: Device cannot be null");
        }

        m_Device = device;
        device->GetImmediateContext(m_Context.GetAddressOf());

        DXGI_FORMAT format = GetDXGIFormat(desc.format);

        // Create texture resource
        bool created = false;
        if (desc.type == TextureType::Texture3D) {
            created = CreateTexture3D(desc);
        }
        else {
            created = CreateTexture2D(desc);
        }

        if (!created) {
            throw std::runtime_error("D3D11Texture: Failed to create texture resource");
        }

        // Create views
        if (!CreateViews(desc, format)) {
            throw std::runtime_error("D3D11Texture: Failed to create texture views");
        }

        // Set debug name
        if (!desc.debugName.empty()) {
            SetDebugName(desc.debugName);
        }
    }

    bool D3D11Texture::CreateTexture2D(const TextureDesc& desc) {
        DXGI_FORMAT format = GetDXGIFormat(desc.format);

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = desc.width;
        texDesc.Height = desc.height;
        texDesc.MipLevels = desc.mipLevels;
        texDesc.Format = format;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        // Add bind flags based on usage
        if (desc.isRenderTarget) {
            texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        }
        if (desc.isDepthStencil) {
            texDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
        }
        if (desc.mipLevels > 1) {
            texDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
        }

        // Handle texture arrays and cubemaps
        if (desc.type == TextureType::TextureCube) {
            texDesc.ArraySize = 6 * desc.arraySize;
            texDesc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
        }
        else {
            texDesc.ArraySize = desc.arraySize;
        }

        // Handle initial data
        D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
        std::vector<D3D11_SUBRESOURCE_DATA> initDataArray;

        if (desc.initialData) {
            uint32_t numSubresources = texDesc.ArraySize * texDesc.MipLevels;
            initDataArray.resize(numSubresources);

            uint32_t bytesPerPixel = GetBytesPerPixel(desc.format);
            const uint8_t* srcData = static_cast<const uint8_t*>(desc.initialData);
            size_t offset = 0;

            for (uint32_t arraySlice = 0; arraySlice < texDesc.ArraySize; ++arraySlice) {
                uint32_t width = desc.width;
                uint32_t height = desc.height;

                for (uint32_t mip = 0; mip < texDesc.MipLevels; ++mip) {
                    uint32_t subresourceIndex = mip + arraySlice * texDesc.MipLevels;

                    initDataArray[subresourceIndex].pSysMem = srcData + offset;
                    initDataArray[subresourceIndex].SysMemPitch = width * bytesPerPixel;
                    initDataArray[subresourceIndex].SysMemSlicePitch = 0;

                    offset += width * height * bytesPerPixel;

                    // Calculate next mip dimensions
                    width = std::max(1u, width / 2);
                    height = std::max(1u, height / 2);
                }
            }

            pInitData = initDataArray.data();
        }

        HRESULT hr = m_Device->CreateTexture2D(&texDesc, pInitData, m_Texture2D.GetAddressOf());
        return SUCCEEDED(hr);
    }

    bool D3D11Texture::CreateTexture3D(const TextureDesc& desc) {
        DXGI_FORMAT format = GetDXGIFormat(desc.format);

        D3D11_TEXTURE3D_DESC texDesc = {};
        texDesc.Width = desc.width;
        texDesc.Height = desc.height;
        texDesc.Depth = desc.depth;
        texDesc.MipLevels = desc.mipLevels;
        texDesc.Format = format;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        if (desc.isRenderTarget) {
            texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        }

        // Handle initial data
        D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
        D3D11_SUBRESOURCE_DATA initData = {};

        if (desc.initialData) {
            uint32_t bytesPerPixel = GetBytesPerPixel(desc.format);
            initData.pSysMem = desc.initialData;
            initData.SysMemPitch = desc.width * bytesPerPixel;
            initData.SysMemSlicePitch = desc.width * desc.height * bytesPerPixel;
            pInitData = &initData;
        }

        HRESULT hr = m_Device->CreateTexture3D(&texDesc, pInitData, m_Texture3D.GetAddressOf());
        return SUCCEEDED(hr);
    }

    bool D3D11Texture::CreateViews(const TextureDesc& desc, DXGI_FORMAT format) {
        HRESULT hr;

        // Create Shader Resource View (SRV)
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;

        if (desc.type == TextureType::Texture3D) {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MipLevels = desc.mipLevels;
            srvDesc.Texture3D.MostDetailedMip = 0;

            hr = m_Device->CreateShaderResourceView(m_Texture3D.Get(), &srvDesc, m_SRV.GetAddressOf());
        }
        else {
            if (desc.type == TextureType::TextureCube) {
                if (desc.arraySize > 1) {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                    srvDesc.TextureCubeArray.NumCubes = desc.arraySize;
                    srvDesc.TextureCubeArray.MipLevels = desc.mipLevels;
                    srvDesc.TextureCubeArray.MostDetailedMip = 0;
                }
                else {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                    srvDesc.TextureCube.MipLevels = desc.mipLevels;
                    srvDesc.TextureCube.MostDetailedMip = 0;
                }
            }
            else if (desc.type == TextureType::Texture2DArray) {
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                srvDesc.Texture2DArray.ArraySize = desc.arraySize;
                srvDesc.Texture2DArray.MipLevels = desc.mipLevels;
                srvDesc.Texture2DArray.MostDetailedMip = 0;
            }
            else {
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = desc.mipLevels;
                srvDesc.Texture2D.MostDetailedMip = 0;
            }

            hr = m_Device->CreateShaderResourceView(m_Texture2D.Get(), &srvDesc, m_SRV.GetAddressOf());
        }

        if (FAILED(hr)) {
            return false;
        }

        // Create Render Target View (RTV) if needed
        if (desc.isRenderTarget) {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = format;

            if (desc.type == TextureType::Texture3D) {
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                rtvDesc.Texture3D.MipSlice = 0;
                rtvDesc.Texture3D.FirstWSlice = 0;
                rtvDesc.Texture3D.WSize = desc.depth;
                hr = m_Device->CreateRenderTargetView(m_Texture3D.Get(), &rtvDesc, m_RTV.GetAddressOf());
            }
            else {
                if (desc.arraySize > 1) {
                    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                    rtvDesc.Texture2DArray.MipSlice = 0;
                    rtvDesc.Texture2DArray.ArraySize = desc.arraySize;
                }
                else {
                    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                    rtvDesc.Texture2D.MipSlice = 0;
                }
                hr = m_Device->CreateRenderTargetView(m_Texture2D.Get(), &rtvDesc, m_RTV.GetAddressOf());
            }

            if (FAILED(hr)) {
                return false;
            }
        }

        // Create Depth Stencil View (DSV) if needed
        if (desc.isDepthStencil && m_Texture2D) {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = format;

            if (desc.arraySize > 1) {
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                dsvDesc.Texture2DArray.MipSlice = 0;
                dsvDesc.Texture2DArray.ArraySize = desc.arraySize;
            }
            else {
                dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                dsvDesc.Texture2D.MipSlice = 0;
            }

            hr = m_Device->CreateDepthStencilView(m_Texture2D.Get(), &dsvDesc, m_DSV.GetAddressOf());
            if (FAILED(hr)) {
                return false;
            }
        }

        return true;
    }

    bool D3D11Texture::Update(const void* data, uint32_t mipLevel, uint32_t arraySlice) {
        if (!data) {
            return false;
        }

        if (m_Texture2D) {
            uint32_t subresource = D3D11CalcSubresource(mipLevel, arraySlice, m_Desc.mipLevels);

            uint32_t width = CalculateMipSize(m_Desc.width, mipLevel);
            uint32_t height = CalculateMipSize(m_Desc.height, mipLevel);
            uint32_t rowPitch = width * GetBytesPerPixel(m_Desc.format);

            m_Context->UpdateSubresource(
                m_Texture2D.Get(),
                subresource,
                nullptr,  // Update entire subresource
                data,
                rowPitch,
                0
            );
            return true;
        }

        if (m_Texture3D) {
            uint32_t subresource = mipLevel;

            uint32_t width = CalculateMipSize(m_Desc.width, mipLevel);
            uint32_t height = CalculateMipSize(m_Desc.height, mipLevel);
            uint32_t depth = CalculateMipSize(m_Desc.depth, mipLevel);
            uint32_t rowPitch = width * GetBytesPerPixel(m_Desc.format);
            uint32_t depthPitch = rowPitch * height;

            m_Context->UpdateSubresource(
                m_Texture3D.Get(),
                subresource,
                nullptr,
                data,
                rowPitch,
                depthPitch
            );
            return true;
        }

        return false;
    }

    bool D3D11Texture::GenerateMips() {
        if (!m_SRV) {
            return false;
        }

        if (m_Desc.mipLevels <= 1) {
            return false;  // No mips to generate
        }

        m_Context->GenerateMips(m_SRV.Get());
        return true;
    }

    bool D3D11Texture::IsValid() const {
        bool hasTexture = (m_Texture2D != nullptr) || (m_Texture3D != nullptr);
        bool hasSRV = (m_SRV != nullptr);
        return hasTexture && hasSRV;
    }

    size_t D3D11Texture::GetMemoryUsage() const {
        uint32_t bytesPerPixel = GetBytesPerPixel(m_Desc.format);
        size_t totalSize = 0;

        if (m_Desc.type == TextureType::Texture3D) {
            // 3D texture with mips
            uint32_t width = m_Desc.width;
            uint32_t height = m_Desc.height;
            uint32_t depth = m_Desc.depth;

            for (uint32_t mip = 0; mip < m_Desc.mipLevels; ++mip) {
                totalSize += width * height * depth * bytesPerPixel;
                width = std::max(1u, width / 2);
                height = std::max(1u, height / 2);
                depth = std::max(1u, depth / 2);
            }
        }
        else {
            // 2D texture with mips and arrays
            uint32_t width = m_Desc.width;
            uint32_t height = m_Desc.height;

            for (uint32_t mip = 0; mip < m_Desc.mipLevels; ++mip) {
                totalSize += width * height * bytesPerPixel;
                width = std::max(1u, width / 2);
                height = std::max(1u, height / 2);
            }

            // Multiply by array size (or 6 for cubemaps)
            if (m_Desc.type == TextureType::TextureCube) {
                totalSize *= 6 * m_Desc.arraySize;
            }
            else {
                totalSize *= m_Desc.arraySize;
            }
        }

        return totalSize;
    }

    void D3D11Texture::SetDebugName(const std::string& name) {
        m_DebugName = name;

        if (m_Texture2D) {
            m_Texture2D->SetPrivateData(WKPDID_D3DDebugObjectName,
                static_cast<UINT>(name.length()), name.c_str());
        }
        if (m_Texture3D) {
            m_Texture3D->SetPrivateData(WKPDID_D3DDebugObjectName,
                static_cast<UINT>(name.length()), name.c_str());
        }
        if (m_SRV) {
            std::string srvName = name + "_SRV";
            m_SRV->SetPrivateData(WKPDID_D3DDebugObjectName,
                static_cast<UINT>(srvName.length()), srvName.c_str());
        }
        if (m_RTV) {
            std::string rtvName = name + "_RTV";
            m_RTV->SetPrivateData(WKPDID_D3DDebugObjectName,
                static_cast<UINT>(rtvName.length()), rtvName.c_str());
        }
        if (m_DSV) {
            std::string dsvName = name + "_DSV";
            m_DSV->SetPrivateData(WKPDID_D3DDebugObjectName,
                static_cast<UINT>(dsvName.length()), dsvName.c_str());
        }
    }
}