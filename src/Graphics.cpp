#include "Graphics.h"
#include <d3d11sdklayers.h>


Graphics::Graphics(HWND hwnd, int width, int height, bool enableWireFrame)
    :
    hwnd(hwnd),
    width(width),
    height(height),
    enableWireFrame(enableWireFrame),
    m_Camera(nullptr)
{
    if (!Intialize())
    {
        MessageBox(hwnd,L"Failed to initialize direct3D", L"ERROR", MB_OK | MB_ICONERROR);
    }
}

Graphics::~Graphics()
{
}

const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& Graphics::GetContext() const
{
    return pContext;
}

const Microsoft::WRL::ComPtr<ID3D11Device>& Graphics::GetDevice() const
{
    return pDevice;
}



bool Graphics::Intialize()
{
    using namespace Microsoft::WRL;

    DXGI_SWAP_CHAIN_DESC sd = {};
    ZeroMemory(&sd, sizeof(DXGI_SWAP_CHAIN_DESC));

    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 1;
    sd.OutputWindow = hwnd;
    sd.Windowed = true;
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
 
    if (FAILED(hr))
    {
        PrintError(hr);
    }

    //back buffer
   ComPtr<ID3D11Resource> pBackBuffer;
   hr= pSwapChain->GetBuffer(0,__uuidof(ID3D11Resource),&pBackBuffer);
   hr= pDevice->CreateRenderTargetView(pBackBuffer.Get(),nullptr,pRenderTarget.GetAddressOf());

    //depth stencil
   D3D11_DEPTH_STENCIL_DESC dsDesc = {};
   dsDesc.DepthEnable = TRUE;
   dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
   dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
  hr= pDevice->CreateDepthStencilState(&dsDesc, DSLessEqual.GetAddressOf());

        //bind the depth state
  pContext->OMSetDepthStencilState(DSLessEqual.Get(), 1);

   // create depth stensil texture
   ComPtr<ID3D11Texture2D> pDepthStencil;

   D3D11_TEXTURE2D_DESC descDepth = {};
   descDepth.Width = (float)width;
   descDepth.Height = (float)height;
   descDepth.MipLevels = 1u;
   descDepth.ArraySize = 1u;
   descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
   descDepth.SampleDesc.Count = 1u;
   descDepth.SampleDesc.Quality = 0u;
   descDepth.Usage = D3D11_USAGE_DEFAULT;
   descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
   
   hr = pDevice->CreateTexture2D(&descDepth, nullptr, pDepthStencil.GetAddressOf());

       //create depth view
   D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
   descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
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

   //blending
   D3D11_BLEND_DESC blendDesc;
   ZeroMemory(&blendDesc, sizeof(blendDesc));

   D3D11_RENDER_TARGET_BLEND_DESC rtbd;
   ZeroMemory(&rtbd, sizeof(rtbd));

   rtbd.BlendEnable = true;
   rtbd.SrcBlend = D3D11_BLEND_SRC_COLOR;
   rtbd.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
   rtbd.BlendOp = D3D11_BLEND_OP_ADD;
   rtbd.SrcBlendAlpha = D3D11_BLEND_ONE;
   rtbd.DestBlendAlpha = D3D11_BLEND_ZERO;
   rtbd.BlendOpAlpha = D3D11_BLEND_OP_ADD;
   rtbd.RenderTargetWriteMask = D3D10_COLOR_WRITE_ENABLE_ALL;

   blendDesc.AlphaToCoverageEnable = false;
   blendDesc.RenderTarget[0] = rtbd;
   pDevice->CreateBlendState(&blendDesc, Tranparency.GetAddressOf());


   D3D11_RASTERIZER_DESC wfDesc;
   ZeroMemory(&wfDesc, sizeof(D3D11_RASTERIZER_DESC));
   if (enableWireFrame)
   {
       wfDesc.FillMode = D3D11_FILL_WIREFRAME;
       wfDesc.CullMode = D3D11_CULL_NONE;
   }
   else
   {
       wfDesc.FillMode = D3D11_FILL_SOLID;
       wfDesc.CullMode = D3D11_CULL_BACK;
   }
   wfDesc.FrontCounterClockwise = true;
   hr = pDevice->CreateRasterizerState(&wfDesc, wireFrame.GetAddressOf());


   wfDesc.FrontCounterClockwise = false;
   hr = pDevice->CreateRasterizerState(&wfDesc, CWcullMode.GetAddressOf());

   wfDesc.CullMode = D3D11_CULL_FRONT;
   hr = pDevice->CreateRasterizerState(&wfDesc, RSCullNone.GetAddressOf());

   
    return true;
}

void Graphics::Resize(int newWidth, int newHeight)
{
    pContext->OMSetRenderTargets(0, nullptr, nullptr);
    pRenderTarget.Reset();
    pDsv.Reset();

    // 2) Resize the swap‐chain buffers
    HRESULT hr = pSwapChain->ResizeBuffers(
        1,                // buffer count
        newWidth,
        newHeight,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        0                 // flags
    );
    if (FAILED(hr))
        PrintError(hr);

    // 3) Recreate render‐target view
    Microsoft::WRL::ComPtr<ID3D11Resource> backBuffer;
    hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Resource), &backBuffer);
    if (FAILED(hr))
        PrintError(hr);

    hr = pDevice->CreateRenderTargetView(
        backBuffer.Get(),
        nullptr,
        pRenderTarget.GetAddressOf()
    );
    if (FAILED(hr))
        PrintError(hr);

    // 4) Recreate depth‐stencil buffer & view
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = newWidth;
    descDepth.Height = newHeight;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
    hr = pDevice->CreateTexture2D(
        &descDepth,
        nullptr,
        depthTex.GetAddressOf()
    );
    if (FAILED(hr))
        PrintError(hr);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = descDepth.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    hr = pDevice->CreateDepthStencilView(
        depthTex.Get(),
        &dsvDesc,
        pDsv.GetAddressOf()
    );
    if (FAILED(hr))
        PrintError(hr);

    // 5) Bind the new views
    pContext->OMSetRenderTargets(1, pRenderTarget.GetAddressOf(), pDsv.Get());

    // 6) Update the viewport
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = static_cast<float>(newWidth);
    vp.Height = static_cast<float>(newHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    pContext->RSSetViewports(1, &vp);

    // 7) Update your stored dimensions
    width = newWidth;
    height = newHeight;
}



HWND Graphics::getHwnd()
{
    return hwnd;
}

void Graphics::ShowMessageBox(const wchar_t* title, const char* message)
{
    MessageBox(nullptr,(LPCWSTR) message, title, MB_OK | MB_ICONERROR);
}

void Graphics::PrintError(HRESULT ghr)
{
  //  ID3D11InfoQueue* pInfoQueue = nullptr;
  //
  //  // Query the debug interface
  //  
  //   ghr = D3D11GetDebugInterface(__uuidof(ID3D11InfoQueue), (void**)&pInfoQueue);
  //   
  //  if (SUCCEEDED(ghr)) {
  //      // Set up the debug message callback
  //      pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
  //      pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
  //
  //      // Loop through the messages
  //      SIZE_T numMessages = pInfoQueue->GetNumStoredMessages();
  //      for (SIZE_T i = 0; i < numMessages; ++i) {
  //          SIZE_T messageLength = 0;
  //          pInfoQueue->GetMessage(i, nullptr, &messageLength);
  //
  //          D3D11_MESSAGE* pMessage = (D3D11_MESSAGE*)malloc(messageLength);
  //          if (pMessage) {
  //              pInfoQueue->GetMessage(i, pMessage, &messageLength);
  //
  //              // Display the message in a message box
  //              ShowMessageBox(L"Direct3D Error", pMessage->pDescription);
  //
  //              free(pMessage);
  //          }
  //      }
  //
  //      // Release the info queue
  //      pInfoQueue->Release();
  //  }
}

void Graphics::ClearDepthColor(float red, float green, float blue)
{
    const float color[4] = { red,green,blue,1.0f };
    pContext->ClearRenderTargetView(pRenderTarget.Get(), color);
    pContext->ClearDepthStencilView(pDsv.Get(), D3D11_CLEAR_DEPTH| D3D11_CLEAR_STENCIL, 1.0f, 0u);
    //reset the shader
    pContext->VSSetShader(0, 0, 0);
    pContext->PSSetShader(0, 0, 0);

    //Set our Render Target
    pContext->OMSetRenderTargets(1u, pRenderTarget.GetAddressOf(), pDsv.Get());

    //Set the default blend state (no blending) for opaque objects
    pContext->OMSetBlendState(Tranparency.Get(), 0, 0xffffffff);

}

void Graphics::Draw(UINT indexCount)
{
    pContext->RSSetState(wireFrame.Get());
    pContext->DrawIndexed(indexCount, 0,0);

}

void Graphics::DrawSkybox(UINT indexCount)
{
    
    pContext->OMSetDepthStencilState(DSLessEqual.Get(), 1);
    pContext->RSSetState(RSCullNone.Get());
    pContext->DrawIndexed(indexCount,0, 0);

   
}

void Graphics::End()
{
    HRESULT hr;
    pContext->OMSetDepthStencilState(nullptr, 0);
    hr = pSwapChain->Present(0u, 0u);
}

void Graphics::SetCamera(const std::shared_ptr<Camera>& camera)
{
    m_Camera = camera;
}

const Camera& Graphics::GetCamera() const
{
    return *m_Camera.get();
}

