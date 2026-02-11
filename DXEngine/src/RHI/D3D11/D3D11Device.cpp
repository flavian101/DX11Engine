#include "dxpch.h"
#include "D3D11Device.h"
#include "D3D11Pipeline.h"
#include "D3D11Shader.h"
#include "D3D11Buffer.h"
#include "D3D11Texture.h"
#include "D3D11CommandBuffer.h"
#include "D3D11Sampler.h"

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
		auto buffer = std::make_shared<D3D11Buffer>(m_Device.Get(), desc);

		//track Memory
		m_MemoryStats.totalAllocated += desc.size;
		m_MemoryStats.totalUsed += desc.size;
		m_MemoryStats.bufferMemory += desc.size;
		m_MemoryStats.bufferCount++;

		return buffer;
	}

	std::shared_ptr<ITexture> D3D11Device::CreateTexture(const TextureDesc& desc) {

		auto texture = std::make_shared<D3D11Texture>(m_Device.Get(), desc);

		// Calculate texture memory (approximate)
		size_t textureMemory = texture->GetMemoryUsage();

		m_MemoryStats.totalAllocated += textureMemory;
		m_MemoryStats.totalUsed += textureMemory;
		m_MemoryStats.textureMemory += textureMemory;
		m_MemoryStats.textureCount++;

		return texture;
	}

	std::shared_ptr<IShader> D3D11Device::CreateShader(const ShaderDesc& desc) {
		return std::make_shared<D3D11Shader>(m_Device.Get(), desc);
	}

	std::shared_ptr<IPipeline> D3D11Device::CreatePipeline(const PipelineDesc& desc) {
		return std::make_shared<D3D11Pipeline>(m_Device.Get(), desc);
	}

	std::shared_ptr<ISampler> D3D11Device::CreateSampler(const SamplerDesc& desc)
	{
		try
		{
			auto sampler = std::make_shared<D3D11Sampler>(m_Device.Get(), desc);
			//track memory usage
			m_MemoryStats.totalAllocated += sampler->GetMemoryUsage();
			return sampler;
		}
		catch (const std::exception& e)
		{
			OutputDebugStringA(("Failed to create sampler: " + std::string(e.what()) + "\n").c_str());
			return nullptr;
		}
	}

	std::shared_ptr<ICommandBuffer> D3D11Device::CreateCommandBuffer() {
		return std::make_shared<D3D11CommandBuffer>(m_Context.Get());
	}

	std::shared_ptr<IRenderPass> D3D11Device::CreateRenderPass(const RenderPassDesc& desc)
	{
		//D3D11 does not have explicit render passes, so we return an empty implementation
		OutputDebugStringA("D3D11Device::CreateRenderPass - Render passes are implicit in D3D11\n");
		return nullptr;
	}

	std::shared_ptr<IFence> D3D11Device::CreateFence(const FenceDesc& desc)
	{
		//D3D11 does not have fences, but we can create a dummy implementation if needed
		//that uses ID3D11Query for synchronization
		OutputDebugStringA("D3D11Device::CreateFence - Creating stub fence (D3D11 limitation)\n");

		// TODO: Implement D3D11Fence using ID3D11Query
		return nullptr;
	}

	std::shared_ptr<IQuery> D3D11Device::CreateQuery(const QueryDesc& desc)
	{
		// TODO: Implement D3D11Query using ID3D11Query
		OutputDebugStringA("D3D11Device::CreateQuery - Not yet implemented\n");
		return nullptr;
	}


	void D3D11Device::Submit(ICommandBuffer* cmd)
	{
		// In D3D11, commands are immediately executed, so nothing to do here
	}

	void D3D11Device::Submit(ICommandBuffer** cmds, uint32_t count, IFence* signalFence)
	{
		//D3D11 note: commands execute immediately, so this is mostly a no-op
		// we process them for API compatibility
		for (uint32_t i = 0; i < count; i++)
		{
			Submit(cmds[i]); //call single submit for each command buffer
		}

		//signal Fence if provieded 
		if (signalFence)
		{
			//D3D11 doesn't support fences natively(true fences), but we can signal completion
			//since everything is immediate
			signalFence->Signal(signalFence->GetCompletedValue() + 1);
		}
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

	IGraphicsDevice::MemoryStats D3D11Device::GetMemoryStats() const
	{
		//query DXGI adapter for Memory info
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		m_Device.As(&dxgiDevice);

		if(dxgiDevice)
		{
			ComPtr<IDXGIAdapter> adapter;
			dxgiDevice->GetAdapter(&adapter);

			if (adapter)
			{
				DXGI_ADAPTER_DESC adapterDesc;
				adapter->GetDesc(&adapterDesc);

				m_MemoryStats.totalAllocated = adapterDesc.DedicatedVideoMemory;
				// Note: D3D11 doesn't expose current usage directly
				// approximation
			}
		}
		return m_MemoryStats;
	}

	uint64_t D3D11Device::GetTimestampFrequency() const
	{
		// D3D11 timestamp frequency
			// Query using ID3D11Query with D3D11_QUERY_TIMESTAMP_DISJOINT

		D3D11_QUERY_DESC queryDesc = {};
		queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

		ComPtr<ID3D11Query> query;
		HRESULT hr = m_Device->CreateQuery(&queryDesc, &query);

		if (SUCCEEDED(hr))
		{
			m_Context->Begin(query.Get());
			m_Context->End(query.Get());

			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
			while (m_Context->GetData(query.Get(), &disjointData, sizeof(disjointData), 0) == S_FALSE)
			{
				// Wait for data
			}

			return disjointData.Frequency;
		}

		// Fallback: typical value is 10MHz
		return 10000000;
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