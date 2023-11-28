#include "Graphics.h"


Graphics::Graphics(HWND hwnd, int width, int height)
    :
    hwnd(hwnd),
    width(width),
    height(height)
{
    if (!Intialize())
    {
        MessageBox(hwnd,L"Failed to initialize direct3D", L"ERROR", MB_OK | MB_ICONERROR);
    }
}

Graphics::~Graphics()
{
}

bool Graphics::Intialize()
{
    using namespace Microsoft::WRL;

    DXGI_SWAP_CHAIN_DESC sd = {};

    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 0;
    sd.BufferDesc.RefreshRate.Denominator = 0;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 1;
    sd.OutputWindow = hwnd;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = 0;

    HRESULT hr;

    hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_DEBUG,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &pSwapChain,
        &pDevice,
        nullptr,
        &pContext
    );

    //back buffer
   ComPtr<ID3D11Resource> pBackBuffer;
   hr= pSwapChain->GetBuffer(0,__uuidof(ID3D11Resource),&pBackBuffer);
   hr= pDevice->CreateRenderTargetView(pBackBuffer.Get(),nullptr,pRenderTarget.GetAddressOf());

    //depth stencil
   D3D11_DEPTH_STENCIL_DESC dsDesc = {};
   dsDesc.DepthEnable = TRUE;
   dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
   dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
   ComPtr<ID3D11DepthStencilState> depthState;
  hr= pDevice->CreateDepthStencilState(&dsDesc, depthState.GetAddressOf());
        //bind the depth state
  pContext->OMSetDepthStencilState(depthState.Get(), 1);

   // create depth stensil texture
   ComPtr<ID3D11Texture2D> pDepthStencil;

   D3D11_TEXTURE2D_DESC descDepth = {};
   descDepth.Width = (float)width;
   descDepth.Height = (float)height;
   descDepth.MipLevels = 1u;
   descDepth.ArraySize = 1u;
   descDepth.Format = DXGI_FORMAT_D32_FLOAT;
   descDepth.SampleDesc.Count = 1u;
   descDepth.SampleDesc.Quality = 0u;
   descDepth.Usage = D3D11_USAGE_DEFAULT;
   descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
   
   hr = pDevice->CreateTexture2D(&descDepth, nullptr, pDepthStencil.GetAddressOf());

       //create depth view
   D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
   descDSV.Format = DXGI_FORMAT_D32_FLOAT;
   descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
   descDSV.Texture2D.MipSlice = 0u;
   pDevice->CreateDepthStencilView(
       pDepthStencil.Get(), &descDSV, pDsv.GetAddressOf());
   // bind depth stensil view to OM
   pContext->OMSetRenderTargets(1u, pRenderTarget.GetAddressOf(), pDsv.Get());

   //viewPort
   D3D11_VIEWPORT vp;
   vp.Width = (float)width;
   vp.Height = (float)height;
   vp.MinDepth = 0.0f;
   vp.MaxDepth = 1.0f;
   vp.TopLeftX = 0.0f;
   vp.TopLeftY = 0.0f;
   pContext->RSSetViewports(1u, &vp);

    return true;
}

void Graphics::ClearDepthColor(float red, float green, float blue)
{
    const float color[] = { red,green,blue,1.0f };
    pContext->ClearRenderTargetView(pRenderTarget.Get(), color);
    pContext->ClearDepthStencilView(pDsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0u);
}

void Graphics::End()
{
    HRESULT hr;
    hr = pSwapChain->Present(1u, 0u);
}
