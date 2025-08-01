#include "CubeMapTexture.h"
#include "stb_image.h"

CubeMapTexture::CubeMapTexture(Graphics& g, const char* filename[6], UINT slot)
    :
    slot(slot)
{
    // Load the cubemap faces from the specified files
    int width, height, channels;
    unsigned char* pData[6];

    for (int i = 0; i < 6; ++i)
    {
        pData[i] = stbi_load(filename[i], &width, &height, &channels, STBI_rgb_alpha);
        if (!pData[i])
        {
            MessageBoxA(NULL, "Failed to load cubemap face", "Error", MB_OK);
            return;
        }
    }

    // Create the cubemap texture
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 6;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;



    HRESULT hr;
    // Copy the cubemap face data to the texture
    D3D11_SUBRESOURCE_DATA subresourceData[6];
    for (int i = 0; i < 6; ++i)
    {
        subresourceData[i].pSysMem = pData[i];
        subresourceData[i].SysMemPitch = width * 4;
        subresourceData[i].SysMemSlicePitch = 0;
    }

    g.GetDevice()->CreateTexture2D(&desc, subresourceData, skyTexture.GetAddressOf());


    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = desc.Format; // Specify the format of the cubemap texture
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE; // Specify that the view is for a cubemap texture
    srvDesc.TextureCube.MipLevels = 1; // Specify the number of mip levels for the cubemap texture
    srvDesc.TextureCube.MostDetailedMip = 0; // Specify the index of the most detailed mip level



    hr = g.GetDevice()->CreateShaderResourceView(skyTexture.Get(), &srvDesc, skyTextureView.GetAddressOf());

    if (FAILED(hr))
    {
        MessageBoxA(NULL, "error creating cube map shader resource view","ERROR",MB_OK);
    }

    // Free the loaded image data
    for (int i = 0; i < 6; ++i)
    {
        stbi_image_free(pData[i]);
    }
}

CubeMapTexture::~CubeMapTexture()
{
    skyTextureView.Reset();
    skyTexture.Reset();
}

void CubeMapTexture::Bind(Graphics& g)
{
    g.GetContext()->PSSetShaderResources(slot, 1, skyTextureView.GetAddressOf());
}
