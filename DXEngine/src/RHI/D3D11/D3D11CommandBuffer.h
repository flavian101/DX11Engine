#pragma once
#include "RHI/GraphicsDevice.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace DXEngine::RHI
{
	/// <summary>
	/// D3D11Command Buffer implementation.
	/// 
	/// IMPORTANT LIMITATIONS:
	/// Direct3D 11 uses an immediate-mode rendering model. Unlike Vulkan or D3D12,
    /// it does NOT support true deferred command recording. This implementation
    /// provides the ICommandBuffer interface for API consistency, but all commands
    /// execute immediately on the device context.
	/// 
	/// IMPLICATIONS:
    /// - Begin()/End() are no-ops for API compatibility
    /// - Commands are NOT thread-safe (single device context)
    /// - No multi-threaded command recording
    /// - Cannot replay/reuse command buffers
    /// 
    /// For true deferred command recording, use D3D12 or Vulkan backend.
    /// 
    /// USAGE PATTERN:
    /// ```cpp
    /// auto cmd = device->CreateCommandBuffer();
    /// cmd->Begin();  // No-op, but call for API consistency
    /// cmd->SetPipeline(pipeline);  // Executes immediately
    /// cmd->Draw(vertexCount);      // Executes immediately
    /// cmd->End();    // No-op
    /// device->Submit(cmd.get());   // No-op (already executed)
    /// ```
	/// </summary>
	using Microsoft::WRL::ComPtr;
	class D3D11CommandBuffer :public ICommandBuffer
	{
	public:
		D3D11CommandBuffer(ComPtr<ID3D11DeviceContext> context);
		~D3D11CommandBuffer() override = default;

		void Begin() override;
		void End() override;
		bool IsRecording() const override { return m_IsRecording; }
		void Reset() override;


		//Binding
		void SetPipeline(IPipeline* pipeline) override;
		void SetVertexBuffer(IBuffer* buffer, uint32_t slot = 0, uint32_t offset = 0) override;
		void SetIndexBuffer(IBuffer* buffer, uint32_t slot = 0, uint32_t offset = 0) = 0;
		void SetConstantBuffer(IBuffer* buffer, uint32_t slot = 0, ShaderStage stage = ShaderStage::Vertex) override;
		void SetTexture(ITexture* texture, uint32_t slot = 0, ShaderStage stage = ShaderStage::Pixel) override;
		void SetSampler(ISampler* sampler, uint32_t slot = 0, ShaderStage stage = ShaderStage::Pixel) override;

		//Multiple Resource binding
		virtual void SetVertexBuffers(IBuffer** buffers, uint32_t count, uint32_t startSlot = 0) override;
		virtual void SetTextures(ITexture** textures, uint32_t count, uint32_t startSlot = 0, ShaderStage stage = ShaderStage::Pixel)  override;


		//Drawing Commands
		virtual void Draw(uint32_t vertexCount, uint32_t startVertex = 0)override;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseIndex = 0)override;
		virtual void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex = 0, uint32_t startInstance = 0)override;
		virtual void DrawIndexedInstanced(uint32_t indexCount,
			uint32_t instanceCount, uint32_t startIndex,
			int32_t baseVertex = 0, uint32_t startInstance = 0)override;

		//Indirect drawing
		virtual void DrawIndirect(IBuffer* argsBuffer, uint32_t offset = 0)override;
		virtual void DrawIndexedIndirect(IBuffer* argsBuffer, uint32_t offset = 0)override;

		//render Targets
		virtual void SetRenderTarget(ITexture* colorTarget, ITexture* depthTarget = nullptr)override;
		virtual void SetRenderTargets(ITexture** colorTargets, uint32_t count, ITexture* depthTarget = nullptr)override;
		virtual void ClearRenderTarget(ITexture* target, float r, float g, float b, float a) override;
		virtual void ClearDepthStencil(ITexture* target, float depth, uint8_t stencil = 0) override;

		//Render pass support
		virtual void BeginRenderPass(IRenderPass* renderPass, ITexture** targets, uint32_t targetCount)override;
		virtual void EndRenderPass()override;

		//viewPort and scissors
		virtual void SetViewport(const ViewportDesc& viewport)override;
		virtual void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height)override;

		//Resource barriers/transitions (for D3D12/Vulkan)
		virtual void TransitionBarrier(ITexture* texture, ResourceState before, ResourceState after)override;
		virtual void TransitionBarrier(IBuffer* buffer, ResourceState before, ResourceState after)override;

		//Copy operations
		virtual void CopyBuffer(IBuffer* src, IBuffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size)override;
		virtual void CopyTexture(ITexture* src, ITexture* dst) = 0;
		virtual void CopyBufferToTexture(IBuffer* src, ITexture* dst, uint32_t mipLevel = 0, uint32_t arraySlice = 0)override;
		virtual void CopyTextureToBuffer(ITexture* src, IBuffer* dst, uint32_t mipLevel = 0, uint32_t arraySlice = 0)override;

		//Compute shader dispatch
		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)override;
		virtual void DispatchIndirect(IBuffer* argsBuffer, uint32_t offset = 0)override;

		//Query operations
		virtual void BeginQuery(IQuery* query, uint32_t index = 0) override;
		virtual void EndQuery(IQuery* query, uint32_t index = 0) override;
		virtual void WriteTimestamp(IQuery* query, PipelineStage stage, uint32_t index = 0)override;

		//Push constants (for small, frequent updates)
		virtual void PushConstants(const void* data, uint32_t size, uint32_t offset = 0)override;


	private:
		ComPtr<ID3D11DeviceContext> m_Context;
		bool m_IsRecording = false;
	};
}
