#pragma once
#include "RHI/GraphicsDevice.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace DXEngine::RHI
{
	using Microsoft::WRL::ComPtr;
	class D3D11CommandBuffer :public ICommandBuffer
	{
	public:
		D3D11CommandBuffer(ID3D11DeviceContext* context);
		~D3D11CommandBuffer() override = default;

		void Begin() override;
		void End() override;
		bool IsRecording() const override { return m_IsRecording; }


		//Binding
		void SetPipeline(IPipeline* pipeline) override;
		void SetVertexBuffer(IBuffer* buffer, uint32_t slot, uint32_t offset) override;
		void SetIndexBuffer(IBuffer* buffer, uint32_t slot = 0, uint32_t offset = 0) override;
		void SetConstantBuffer(IBuffer* buffer, uint32_t slot, ShaderStage stage) override;
		void SetTexture(ITexture* texture, uint32_t slot, ShaderStage stage) override;


		//drawing 
		void Draw(uint32_t vertexCount, uint32_t startVertex) override;
		void DrawIndexed(uint32_t indexCount, uint32_t startIndex,
			int32_t baseVertex) override;
		void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount,
			uint32_t startVertex, uint32_t startInstance) override;
		void DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
			uint32_t startIndex, int32_t baseVertex,
			uint32_t startInstance) override;
		
		//Render Targets
		void SetRenderTarget(ITexture* colorTarget, ITexture* depthTarget) override;
		void ClearRenderTarget(ITexture* target, float r, float g, float b, float a) override;
		void ClearDepthStencil(ITexture* target, float depth, uint8_t stencil) override;

		// Viewport
		void SetViewport(const ViewportDesc& viewport) override;
		void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;


	private:
		ComPtr<ID3D11DeviceContext> m_Context;
		bool m_IsRecording = false;
	};
}
