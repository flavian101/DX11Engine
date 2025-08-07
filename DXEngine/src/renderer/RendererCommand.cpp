#include "dxpch.h"
#include "RendererCommand.h"

namespace DXEngine {

	Microsoft::WRL::ComPtr<ID3D11Device> RenderCommand::s_Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> RenderCommand::s_Context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> RenderCommand::s_SwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderCommand::s_RenderTargetView;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> RenderCommand::s_DepthStencilView;
	Microsoft::WRL::ComPtr<ID3D11BlendState> RenderCommand::s_TransparencyBlendState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> RenderCommand::s_DepthStencilState;
	std::unordered_map<RasterizerMode, Microsoft::WRL::ComPtr<ID3D11RasterizerState>> RenderCommand::s_RasterizerStates;
	HWND RenderCommand::s_WindowHandle;
	int RenderCommand::s_ViewportWidth;
	int RenderCommand::s_ViewportHeight;
	std::shared_ptr<Camera> RenderCommand::s_Camera;
	float RenderCommand::s_ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	void RenderCommand::Init(HWND hwnd, int width, int height)
	{
		s_WindowHandle = hwnd;
		s_ViewportWidth = width;
		s_ViewportHeight = height;

		if (!InitializeD3D())
		{
			MessageBox(hwnd, L"Failed to initialize Direct3D", L"ERROR", MB_OK | MB_ICONERROR);
		}
	}

	void RenderCommand::Shutdown()
	{
		s_Camera.reset();
		s_RasterizerStates.clear();
		s_DepthStencilState.Reset();
		s_TransparencyBlendState.Reset();
		s_DepthStencilView.Reset();
		s_RenderTargetView.Reset();
		s_SwapChain.Reset();
		s_Context.Reset();
		s_Device.Reset();
	}

	bool RenderCommand::InitializeD3D()
	{
		using namespace Microsoft::WRL;

		DXGI_SWAP_CHAIN_DESC sd = {};
		ZeroMemory(&sd, sizeof(DXGI_SWAP_CHAIN_DESC));

		sd.BufferDesc.Width = s_ViewportWidth;
		sd.BufferDesc.Height = s_ViewportHeight;
		sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;
		sd.OutputWindow = s_WindowHandle;
		sd.Windowed = true;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		sd.Flags = 0;

		HRESULT hr = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			D3D11_CREATE_DEVICE_DEBUG,
			nullptr,
			0,
			D3D11_SDK_VERSION,
			&sd,
			&s_SwapChain,
			&s_Device,
			nullptr,
			&s_Context
		);

		if (FAILED(hr))
		{
			PrintError(hr);
			return false;
		}

		// Create back buffer render target view
		ComPtr<ID3D11Resource> backBuffer;
		hr = s_SwapChain->GetBuffer(0, __uuidof(ID3D11Resource), &backBuffer);
		if (FAILED(hr)) return false;

		hr = s_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, s_RenderTargetView.GetAddressOf());
		if (FAILED(hr)) return false;

		// Create depth stencil state
		D3D11_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = TRUE;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hr = s_Device->CreateDepthStencilState(&dsDesc, s_DepthStencilState.GetAddressOf());
		if (FAILED(hr)) return false;

		// Bind the depth state
		s_Context->OMSetDepthStencilState(s_DepthStencilState.Get(), 1);

		// Create depth stencil texture
		ComPtr<ID3D11Texture2D> depthStencil;
		D3D11_TEXTURE2D_DESC descDepth = {};
		descDepth.Width = s_ViewportWidth;
		descDepth.Height = s_ViewportHeight;
		descDepth.MipLevels = 1u;
		descDepth.ArraySize = 1u;
		descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		descDepth.SampleDesc.Count = 1u;
		descDepth.SampleDesc.Quality = 0u;
		descDepth.Usage = D3D11_USAGE_DEFAULT;
		descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		hr = s_Device->CreateTexture2D(&descDepth, nullptr, depthStencil.GetAddressOf());
		if (FAILED(hr)) return false;

		// Create depth view
		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
		descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Texture2D.MipSlice = 0u;
		s_Device->CreateDepthStencilView(depthStencil.Get(), &descDSV, s_DepthStencilView.GetAddressOf());

		// Bind depth stencil view to output merger
		s_Context->OMSetRenderTargets(1u, s_RenderTargetView.GetAddressOf(), s_DepthStencilView.Get());

		// Set viewport
		SetViewport(0, 0, s_ViewportWidth, s_ViewportHeight);

		// Create blend state for transparency
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
		s_Device->CreateBlendState(&blendDesc, s_TransparencyBlendState.GetAddressOf());

		CreateRasterizerStates();
		return true;
	}

	void RenderCommand::SetClearColor(float red, float green, float blue, float alpha)
	{
		s_ClearColor[0] = red;
		s_ClearColor[1] = green;
		s_ClearColor[2] = blue;
		s_ClearColor[3] = alpha;
	}
	void RenderCommand::Clear()
	{
		s_Context->ClearRenderTargetView(s_RenderTargetView.Get(), s_ClearColor);
		s_Context->ClearDepthStencilView(s_DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0u);

		// Reset shaders
		s_Context->VSSetShader(nullptr, nullptr, 0);
		s_Context->PSSetShader(nullptr, nullptr, 0);

		// Set render targets
		s_Context->OMSetRenderTargets(1u, s_RenderTargetView.GetAddressOf(), s_DepthStencilView.Get());

		// Set default blend state
		s_Context->OMSetBlendState(s_TransparencyBlendState.Get(), nullptr, 0xffffffff);

		SetRasterizerMode(RasterizerMode::SolidBackCull);
	}

	void RenderCommand::Present()
	{
		s_Context->OMSetDepthStencilState(nullptr, 0);
		HRESULT hr = s_SwapChain->Present(0u, 0u);
		if (FAILED(hr))
		{
			PrintError(hr);
		}
	}
	void RenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		D3D11_VIEWPORT vp = {};
		vp.TopLeftX = static_cast<float>(x);
		vp.TopLeftY = static_cast<float>(y);
		vp.Width = static_cast<float>(width);
		vp.Height = static_cast<float>(height);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		s_Context->RSSetViewports(1u, &vp);
	}

	void RenderCommand::SetRasterizerMode(RasterizerMode mode)
	{
		auto it = s_RasterizerStates.find(mode);
		if (it != s_RasterizerStates.end())
		{
			s_Context->RSSetState(it->second.Get());
		}
	}

	void RenderCommand::SetDepthLessEqual()
	{
		s_Context->OMSetDepthStencilState(s_DepthStencilState.Get(), 1);
	}

	void RenderCommand::DrawIndexed(uint32_t indexCount)
	{
		s_Context->DrawIndexed(indexCount, 0, 0);

	}

	void RenderCommand::Resize(int newWidth, int newHeight)
	{
		s_Context->OMSetRenderTargets(0, nullptr, nullptr);
		s_RenderTargetView.Reset();
		s_DepthStencilView.Reset();

		// Resize the swap chain buffers
		HRESULT hr = s_SwapChain->ResizeBuffers(
			1,
			newWidth,
			newHeight,
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
		);
		if (FAILED(hr))
		{
			PrintError(hr);
			return;
		}

		// Recreate render target view
		Microsoft::WRL::ComPtr<ID3D11Resource> backBuffer;
		hr = s_SwapChain->GetBuffer(0, __uuidof(ID3D11Resource), &backBuffer);
		if (FAILED(hr)) return;

		hr = s_Device->CreateRenderTargetView(
			backBuffer.Get(),
			nullptr,
			s_RenderTargetView.GetAddressOf()
		);
		if (FAILED(hr)) return;

		// Recreate depth stencil buffer & view
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
		hr = s_Device->CreateTexture2D(&descDepth, nullptr, depthTex.GetAddressOf());
		if (FAILED(hr)) return;

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = descDepth.Format;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
		hr = s_Device->CreateDepthStencilView(
			depthTex.Get(),
			&dsvDesc,
			s_DepthStencilView.GetAddressOf()
		);
		if (FAILED(hr)) return;

		// Bind the new views
		s_Context->OMSetRenderTargets(1, s_RenderTargetView.GetAddressOf(), s_DepthStencilView.Get());

		// Update viewport
		SetViewport(0, 0, newWidth, newHeight);

		// Update stored dimensions
		s_ViewportWidth = newWidth;
		s_ViewportHeight = newHeight;
	}

	Microsoft::WRL::ComPtr<ID3D11Device> RenderCommand::GetDevice()
	{
		return s_Device;
	}

	Microsoft::WRL::ComPtr<ID3D11DeviceContext> RenderCommand::GetContext()
	{
		return s_Context;
	}

	void RenderCommand::SetCamera(const std::shared_ptr<Camera>& camera)
	{
		s_Camera = camera;

	}

	const Camera& RenderCommand::GetCamera()
	{
		return *s_Camera.get();
	}

	void RenderCommand::CreateRasterizerStates()
	{
		D3D11_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.FrontCounterClockwise = false;
		desc.DepthClipEnable = true;

		// Solid, Back-Face Culling
		desc.CullMode = D3D11_CULL_BACK;
		s_Device->CreateRasterizerState(&desc, s_RasterizerStates[RasterizerMode::SolidBackCull].GetAddressOf());

		// Solid, Front-Face Culling
		desc.CullMode = D3D11_CULL_FRONT;
		s_Device->CreateRasterizerState(&desc, s_RasterizerStates[RasterizerMode::SolidFrontCull].GetAddressOf());

		// Solid, No Culling
		desc.CullMode = D3D11_CULL_NONE;
		s_Device->CreateRasterizerState(&desc, s_RasterizerStates[RasterizerMode::SolidNoCull].GetAddressOf());

		// Wireframe
		desc.FillMode = D3D11_FILL_WIREFRAME;
		s_Device->CreateRasterizerState(&desc, s_RasterizerStates[RasterizerMode::Wireframe].GetAddressOf());
	}

	void RenderCommand::PrintError(HRESULT hr)
	{
		// Implementation for error handling - can be expanded
		// For now, just a placeholder
	}

}