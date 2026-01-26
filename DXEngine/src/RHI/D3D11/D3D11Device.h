#pragma once
#include "RHI/GraphicsDevice.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include "D3D11Texture.h"

namespace DXEngine::RHI
{
	using Microsoft::WRL::ComPtr;

	class D3D11Device : public IGraphicsDevice
	{
	public:
		D3D11Device();
		~D3D11Device() override;

		//IGraphicsDevice
		bool Initialize(void* windowHandle, uint32_t width, uint32_t height) override;
		void Shutdown() override;

		std::shared_ptr<IBuffer> CreateBuffer(const BufferDesc& desc) override;
		std::shared_ptr<ITexture> CreateTexture(const TextureDesc& desc) override;
		std::shared_ptr<IShader> CreateShader(const ShaderDesc& desc) override;			///Fix The memory stat calculation 
		std::shared_ptr<IPipeline> CreatePipeline(const PipelineDesc& desc) override;
		std::shared_ptr<ISampler> CreateSampler(const SamplerDesc& desc) override;

		std::shared_ptr<ICommandBuffer> CreateCommandBuffer() override;
		std::shared_ptr<IRenderPass> CreateRenderPass(const RenderPassDesc& desc) override;
		std::shared_ptr<IFence> CreateFence(const FenceDesc& desc) override;
		std::shared_ptr<IQuery> CreateQuery(const QueryDesc& desc) override;

		void Submit(ICommandBuffer* cmd) override;
		void Submit(ICommandBuffer** cmds, uint32_t count, IFence* signalFence = nullptr) override;

		void Present() override;
		void WaitIdle() override;

		GraphicsAPI GetAPI()const override { return GraphicsAPI::DirectX11; }
		const DeviceCapabilities& GetCapabilities()const override { return m_Capabilities; }

		void Resize(uint32_t width, uint32_t height) override;
		ITexture* GetBackBuffer() override { return m_BackBuffer.get(); }
		uint32_t GetBackBufferWidth() const override { return m_Width; }
		uint32_t GetBackBufferHeight() const override { return m_Height; }

		// Internal D3D11 access (for advanced users)
		ID3D11Device* GetD3D11Device() const { return m_Device.Get(); }
		ID3D11DeviceContext* GetD3D11Context() const { return m_Context.Get(); }

		//Device info
		void* GetnativeDevice() const override { return m_Device.Get(); }
		uint32_t GetCurrentBackBufferIndex() const override { return 0; } //D3D11 does not expose this
		MemoryStats GetMemoryStats() const override;
		uint64_t GetTimestampFrequency() const override;

	private:
		bool CreateDeviceAndSwapChain(void* windowHandle, uint32_t width, uint32_t height);
		bool CreateBackBuffer();
		void QueryCapabilities();
	private:
		ComPtr<ID3D11Device> m_Device;
		ComPtr<ID3D11DeviceContext> m_Context;
		ComPtr<IDXGISwapChain> m_SwapChain;

		std::shared_ptr<D3D11Texture> m_BackBuffer;
		std::shared_ptr<D3D11Texture> m_DepthStencil;

		DeviceCapabilities m_Capabilities;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		HWND m_WindowHandle = nullptr;
		mutable MemoryStats m_MemoryStats;
	};
}

