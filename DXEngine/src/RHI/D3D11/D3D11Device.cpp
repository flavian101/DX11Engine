#include "dxpch.h"
#include "D3D11Device.h"
#include "D3D11Pipeline.h"
#include "D3D11Shader.h"
#include "D3D11Buffer.h"
#include "D3D11Texture.h"
#include "D3D11CommandBuffer.h"

namespace DXEngine::RHI
{

	D3D11Device::D3D11Device() = default;

	D3D11Device::~D3D11Device()
	{
		Shutdown();
	}

	bool D3D11Device::Initialize(void* windowHandle, uint32_t width, uint32_t height)
	{
		m_WindowHandle = static_cast<HWND>(windowHandle);
		m_Width = width;
		m_Height = height;
		if (!CreateDeviceAndSwapChain(windowHandle, width, height))
		{
			return false;
		}

		if (!CreateBackBuffer()) {
			return false;
		}

		QueryCapabilities();
		return true;
	}

	void D3D11Device::Shutdown()
	{
		m_BackBuffer.reset();
		m_DepthStencil.reset();
		m_SwapChain.Reset();
		m_Context.Reset();
		m_Device.Reset();
	}

	std::shared_ptr<IBuffer> D3D11Device::CreateBuffer(const BufferDesc& desc) {
		return std::make_shared<D3D11Buffer>(m_Device.Get(), desc);
	}

	std::shared_ptr<ITexture> D3D11Device::CreateTexture(const TextureDesc& desc) {
		return std::make_shared<D3D11Texture>(m_Device.Get(), desc);
	}

	std::shared_ptr<IShader> D3D11Device::CreateShader(const ShaderDesc& desc) {
		return std::make_shared<D3D11Shader>(m_Device.Get(), desc);
	}

	std::shared_ptr<IPipeline> D3D11Device::CreatePipeline(const PipelineDesc& desc) {
		return std::make_shared<D3D11Pipeline>(m_Device.Get(), desc);
	}

	std::shared_ptr<ICommandBuffer> D3D11Device::CreateCommandBuffer() {
		return std::make_shared<D3D11CommandBuffer>(m_Context.Get());
	}


	void D3D11Device::Submit(ICommandBuffer* cmd)
	{
		// In D3D11, commands are immediately executed, so nothing to do here
	}

	void D3D11Device::Present()
	{
		m_SwapChain->Present(0, 0);
	}

	void D3D11Device::WaitIdle()
	{
		// D3D11 doesn't need explicit synchronization
	}

	void D3D11Device::Resize(uint32_t width, uint32_t height)
	{
		m_Width = width;
		m_Height = height;

		m_BackBuffer.reset();
		m_DepthStencil.reset();

		m_SwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		CreateBackBuffer();
	}

	bool D3D11Device::CreateDeviceAndSwapChain(void* windowHandle, uint32_t width, uint32_t height)
	{
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferDesc.Width = width;
		swapChainDesc.BufferDesc.Height = height;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 1;
		swapChainDesc.OutputWindow = static_cast<HWND>(windowHandle);
		swapChainDesc.Windowed = TRUE;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		D3D_FEATURE_LEVEL featureLevel;
		UINT flags = 0;
#ifdef _DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		HRESULT hr = D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
			nullptr, 0, D3D11_SDK_VERSION,
			&swapChainDesc, m_SwapChain.GetAddressOf(),
			m_Device.GetAddressOf(), &featureLevel, m_Context.GetAddressOf()
		);

		return SUCCEEDED(hr);
	}

	bool D3D11Device::CreateBackBuffer()
	{
		ComPtr<ID3D11Texture2D> backBuffer;
		HRESULT hr = m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
		if (FAILED(hr)) return false;

		TextureDesc desc;
		desc.width = m_Width;
		desc.height = m_Height;
		desc.format = TextureFormat::RGBA8_UNORM;
		desc.isRenderTarget = true;
		desc.debugName = "BackBuffer";

		m_BackBuffer = std::make_shared<D3D11Texture>(m_Device.Get(), desc);

		// Create depth buffer
		desc.format = TextureFormat::D24_UNORM_S8_UINT;
		desc.isRenderTarget = false;
		desc.isDepthStencil = true;
		desc.debugName = "DepthStencil";
		m_DepthStencil = std::make_shared<D3D11Texture>(m_Device.Get(), desc);

		return true;
	}

	void D3D11Device::QueryCapabilities()
	{
		m_Capabilities.maxTextureSize = 16384;
		m_Capabilities.maxAnisotropy = 16;
		m_Capabilities.supportsCompute = true;
		m_Capabilities.supportsGeometryShaders = true;
		m_Capabilities.supportsTessellation = true;
		m_Capabilities.deviceName = "D3D11 Device";
	}

	std::unique_ptr<IGraphicsDevice> GraphicsDeviceFactory::Create(GraphicsAPI api) {
		switch (api) {
		case GraphicsAPI::DirectX11:
			return std::make_unique<D3D11Device>();
		default:
			return nullptr;
		}
	}

}