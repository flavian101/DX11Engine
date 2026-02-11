#include "dxpch.h"
#include "D3D11CommandBuffer.h"
#include "D3D11Pipeline.h"
#include "D3D11Shader.h"
#include "D3D11Buffer.h"
#include "D3D11Texture.h"
#include "D3D11Sampler.h"

namespace DXEngine::RHI
{
	D3D11CommandBuffer::D3D11CommandBuffer(ComPtr<ID3D11DeviceContext> context)
		:
		m_Context(context)
	{
	}
	void D3D11CommandBuffer::Begin()
	{
		m_IsRecording = true;// does nothing since D3D11 is immediate context and does not record commands
	}
	void D3D11CommandBuffer::End()
	{
		m_IsRecording = false;
	}
	void D3D11CommandBuffer::Reset()
	{
		// D3D11 doesn't need reset (immediate mode)
		// Just clear recording flag
		m_IsRecording = false;
	}
	void D3D11CommandBuffer::SetPipeline(IPipeline* pipeline)
	{
		auto* d3d11Pipeline = static_cast<D3D11Pipeline*>(pipeline);
		// Executes IMMEDIATELY, not recorded

		m_Context->IASetInputLayout(d3d11Pipeline->GetInputLayout());
		m_Context->RSSetState(d3d11Pipeline->GetRasterizerState());
		m_Context->OMSetDepthStencilState(d3d11Pipeline->GetDepthStencilState(), 0);
		m_Context->OMSetBlendState(d3d11Pipeline->GetBlendState(), nullptr, 0xffffffff);

		//set Shaders from pipeline Desc
		auto& desc = d3d11Pipeline->GetDesc();
		if (desc.vertexShader) {
			auto* vs = static_cast<D3D11Shader*>(desc.vertexShader);
			m_Context->VSSetShader(static_cast<ID3D11VertexShader*>(vs->GetD3D11Shader()), nullptr, 0);
		}
		if (desc.pixelShader) {
			auto* ps = static_cast<D3D11Shader*>(desc.pixelShader);
			m_Context->PSSetShader(static_cast<ID3D11PixelShader*>(ps->GetD3D11Shader()), nullptr, 0);
		}
		if (desc.geometryShader)
		{
			auto* gs = static_cast<D3D11Shader*>(desc.geometryShader);
			m_Context->GSSetShader(static_cast<ID3D11GeometryShader*>(gs->GetD3D11Shader()), nullptr, 0);
		}
	}
	void D3D11CommandBuffer::SetVertexBuffer(IBuffer* buffer, uint32_t slot, uint32_t offset)
	{
		auto* d3d11Buffer = static_cast<D3D11Buffer*>(buffer);
		ID3D11Buffer* vb = d3d11Buffer->GetD3D11Buffer();
		UINT stride = d3d11Buffer->GetStride();
		m_Context->IASetVertexBuffers(slot, 1, &vb, &stride, &offset);
	}
	void D3D11CommandBuffer::SetIndexBuffer(IBuffer* buffer, uint32_t slot, uint32_t offset)
	{
		auto* d3d11Buffer = static_cast<D3D11Buffer*>(buffer);
		m_Context->IASetIndexBuffer(d3d11Buffer->GetD3D11Buffer(), DXGI_FORMAT_R32_UINT, offset);
	}
	void D3D11CommandBuffer::SetConstantBuffer(IBuffer* buffer, uint32_t slot, ShaderStage stage)
	{
		auto* d3d11Buffer = static_cast<D3D11Buffer*>(buffer);
		ID3D11Buffer* cb = d3d11Buffer->GetD3D11Buffer();

		switch (stage)
		{
		case ShaderStage::Vertex:
		{
			m_Context->VSSetConstantBuffers(slot, 1, &cb);
			break;
		}
		case ShaderStage::Pixel:
		{
			m_Context->PSSetConstantBuffers(slot, 1, &cb);
			break;
		}
		case ShaderStage::Hull:
		{
			m_Context->HSSetConstantBuffers(slot, 1, &cb);
			break;
		}
		case ShaderStage::Geometry:
		{
			m_Context->GSSetConstantBuffers(slot, 1, &cb);
			break;
		}
		case ShaderStage::Domain:
		{
			m_Context->DSSetConstantBuffers(slot, 1, &cb);
			break;
		}
		case ShaderStage::Compute:
		{
			m_Context->CSSetConstantBuffers(slot, 1, &cb);
			break;
		}
		}
	}
	void D3D11CommandBuffer::SetTexture(ITexture* texture, uint32_t slot, ShaderStage stage)
	{
		auto* d3d11Texture = static_cast<D3D11Texture*>(texture);
		ID3D11ShaderResourceView* srv = d3d11Texture->GetSRV();

		switch (stage) {
		case ShaderStage::Vertex:
			m_Context->VSSetShaderResources(slot, 1, &srv);
			break;
		case ShaderStage::Pixel:
			m_Context->PSSetShaderResources(slot, 1, &srv);
			break;
		}
	}
	void D3D11CommandBuffer::SetSampler(ISampler* sampler, uint32_t slot, ShaderStage stage)
	{
		if (!sampler)
		{
			OutputDebugStringA("Warning: Setting null sampler. This will unbind any sampler at this slot.\n");
			return;
		}
		auto* d3d11Sampler = static_cast<D3D11Sampler*>(sampler);
		ID3D11SamplerState* ss = d3d11Sampler->GetD3D11Sampler();

		switch (stage)
		{
		case ShaderStage::Vertex:
		{
			m_Context->VSSetSamplers(slot, 1, &ss);
			break;
		}
		case ShaderStage::Pixel:
		{
			m_Context->PSSetSamplers(slot, 1, &ss);
			break;
		}
		case ShaderStage::Geometry:
		{
			m_Context->GSSetSamplers(slot, 1, &ss);
			break;
		}
		case ShaderStage::Hull:
		{
			m_Context->HSSetSamplers(slot, 1, &ss);
			break;
		}
		case ShaderStage::Domain:
		{
			m_Context->DSSetSamplers(slot, 1, &ss);
			break;
		}
		case ShaderStage::Compute:
		{
			m_Context->CSSetSamplers(slot, 1, &ss);
			break;
		}
		default:
			OutputDebugStringA("Warning: Unsupported shader stage for sampler binding.\n");
			break;
		}
	}

	//Multiple resource binding
	void D3D11CommandBuffer::SetVertexBuffers(IBuffer** buffers, uint32_t count, uint32_t startSlot)
	{
		if (!buffers || count == 0)
		{
			OutputDebugStringA("Warning: Setting null vertex buffers. This will unbind any vertex buffers at these slots.\n");
			return;
		}
		std::vector<ID3D11Buffer*> d3dBuffers(count);
		std::vector<UINT> strides(count);
		std::vector<UINT> offsets(count, 0);

		for (uint32_t i = 0; i < count; i++)
		{
			if (buffers[i])
			{
				auto* d3d11Buffer = static_cast<D3D11Buffer*>(buffers[i]);
				d3dBuffers[i] = d3d11Buffer->GetD3D11Buffer();
				strides[i] = d3d11Buffer->GetStride();

			}
			else
			{
				//OutputDebugStringA("Warning: Null vertex buffer at index " + std::to_string(i) + ". This will unbind any vertex buffer at this slot.\n");
				d3dBuffers[i] = nullptr;
				strides[i] = 0;
				offsets[i] = 0;
			}
		}

		m_Context->IASetVertexBuffers(startSlot, count, d3dBuffers.data(), strides.data(), offsets.data());

	}
	void D3D11CommandBuffer::SetTextures(ITexture** textures, uint32_t count, uint32_t startSlot, ShaderStage stage)
	{
		if (!textures || count == 0) return;

		std::vector<ID3D11ShaderResourceView*> srvs(count);

		for (uint32_t i = 0; i < count; ++i)
		{
			if (textures[i])
			{
				auto* d3d11Texture = static_cast<D3D11Texture*>(textures[i]);
				srvs[i] = d3d11Texture->GetSRV();
			}
			else
			{
				srvs[i] = nullptr;
			}
		}

		switch (stage)
		{
		case ShaderStage::Vertex:
			m_Context->VSSetShaderResources(startSlot, count, srvs.data());
			break;
		case ShaderStage::Pixel:
			m_Context->PSSetShaderResources(startSlot, count, srvs.data());
			break;
		case ShaderStage::Geometry:
			m_Context->GSSetShaderResources(startSlot, count, srvs.data());
			break;
		case ShaderStage::Hull:
			m_Context->HSSetShaderResources(startSlot, count, srvs.data());
			break;
		case ShaderStage::Domain:
			m_Context->DSSetShaderResources(startSlot, count, srvs.data());
			break;
		case ShaderStage::Compute:
			m_Context->CSSetShaderResources(startSlot, count, srvs.data());
			break;
		}
	}
	//draw commands 
	void D3D11CommandBuffer::Draw(uint32_t vertexCount, uint32_t startVertex) {
		m_Context->Draw(vertexCount, startVertex);
	}

	void D3D11CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex) {
		m_Context->DrawIndexed(indexCount, startIndex, baseVertex);
	}

	void D3D11CommandBuffer::DrawInstanced(uint32_t vertexCount, uint32_t instanceCount,
		uint32_t startVertex, uint32_t startInstance) {
		m_Context->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
	}

	void D3D11CommandBuffer::DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount,
		uint32_t startIndex, int32_t baseVertex,
		uint32_t startInstance) {
		m_Context->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
	}
	void D3D11CommandBuffer::DrawIndirect(IBuffer* argsBuffer, uint32_t offset)
	{
		if (!argsBuffer) return;

		auto* d3d11Buffer = static_cast<D3D11Buffer*>(argsBuffer);
		m_Context->DrawInstancedIndirect(d3d11Buffer->GetD3D11Buffer(), offset);
	}
	void D3D11CommandBuffer::DrawIndexedIndirect(IBuffer* argsBuffer, uint32_t offset)
	{
		if (!argsBuffer) return;

		auto* d3d11Buffer = static_cast<D3D11Buffer*>(argsBuffer);
		m_Context->DrawIndexedInstancedIndirect(d3d11Buffer->GetD3D11Buffer(), offset);
	}
	//configure render targets and render passes
	void D3D11CommandBuffer::SetRenderTarget(ITexture* colorTarget, ITexture* depthTarget)
	{
		ID3D11RenderTargetView* rtv = colorTarget ? static_cast<D3D11Texture*>(colorTarget)->GetRTV() : nullptr;

		ID3D11DepthStencilView* dsv = depthTarget ?
			static_cast<D3D11Texture*>(depthTarget)->GetDSV() : nullptr;

		m_Context->OMSetRenderTargets(1, &rtv, dsv);
	}
	void D3D11CommandBuffer::SetRenderTargets(ITexture** colorTargets, uint32_t count, ITexture* depthTarget)
	{
		std::vector<ID3D11RenderTargetView*> rtvs(count);

		for (uint32_t i = 0; i < count; ++i)
		{
			if (colorTargets[i])
			{
				auto* d3d11Texture = static_cast<D3D11Texture*>(colorTargets[i]);
				rtvs[i] = d3d11Texture->GetRTV();
			}
			else
			{
				rtvs[i] = nullptr;
			}
		}

		ID3D11DepthStencilView* dsv = nullptr;
		if (depthTarget)
		{
			auto* d3d11DepthTexture = static_cast<D3D11Texture*>(depthTarget);
			dsv = d3d11DepthTexture->GetDSV();
		}

		m_Context->OMSetRenderTargets(count, rtvs.data(), dsv);
	}
	void D3D11CommandBuffer::ClearRenderTarget(ITexture* target, float r, float g, float b, float a)
	{
		float color[4] = { r,g,b,a };
		m_Context->ClearRenderTargetView(static_cast<D3D11Texture*>(target)->GetRTV(), color);
	}
	void D3D11CommandBuffer::ClearDepthStencil(ITexture* target, float depth, uint8_t stencil)
	{
		m_Context->ClearDepthStencilView(static_cast<D3D11Texture*>(target)->GetDSV(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
	}
	void D3D11CommandBuffer::BeginRenderPass(IRenderPass* renderPass, ITexture** targets, uint32_t targetCount)
	{
		// D3D11 doesn't have explicit render passes
		// We can use this as a hint to set render targets and clear them

		if (!renderPass)
		{
			OutputDebugStringA("Warning: BeginRenderPass called with null renderPass (ignored in D3D11)\n");
			return;
		}

		// For now, just document that this is a no-op
		// In the future, could validate render pass compatibility
	}
	void D3D11CommandBuffer::EndRenderPass()
	{
		// D3D11 doesn't have explicit render passes - no-op
	}
	void D3D11CommandBuffer::SetViewport(const ViewportDesc& viewport)
	{
		D3D11_VIEWPORT vp = {};
		vp.TopLeftX = viewport.x;
		vp.TopLeftY = viewport.y;
		vp.Width = viewport.width;
		vp.Height = viewport.height;
		vp.MinDepth = viewport.minDepth;
		vp.MaxDepth = viewport.maxDepth;
		m_Context->RSSetViewports(1, &vp);
	}
	void D3D11CommandBuffer::SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		D3D11_RECT rect = { (LONG)x, (LONG)y, (LONG)(x + width), (LONG)(y + height) };
		m_Context->RSSetScissorRects(1, &rect);
	}
	void D3D11CommandBuffer::TransitionBarrier(ITexture* texture, ResourceState before, ResourceState after)
	{
		// D3D11 handles resource transitions automatically - no-op
#ifdef DX_DEBUG
		OutputDebugStringA("D3D11CommandBuffer::TransitionBarrier - No-op in D3D11 (automatic transitions)\n");
#endif

	}
	void D3D11CommandBuffer::TransitionBarrier(IBuffer* buffer, ResourceState before, ResourceState after)
	{
		// D3D11 handles resource transitions automatically - no-op
#ifdef DX_DEBUG
		OutputDebugStringA("D3D11CommandBuffer::TransitionBarrier - No-op in D3D11 (automatic transitions)\n");
#endif
	}
	void D3D11CommandBuffer::CopyBuffer(IBuffer* src, IBuffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size)
	{
		if (!src)
		{
			OutputDebugStringA("Warning: CopyBuffer called with null source buffer.\n");
			return;
		}
		if (!dst)
		{
			OutputDebugStringA("Warning: CopyBuffer called with null destination buffer.\n");
			return;
		}

		auto* d3d11Src = static_cast<D3D11Buffer*>(src);
		auto* d3d11Dst = static_cast<D3D11Buffer*>(dst);

		D3D11_BOX box = {};
		box.left = srcOffset;
		box.right = srcOffset + size;
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;

		m_Context->CopySubresourceRegion(d3d11Dst->GetD3D11Buffer(), 0, dstOffset, 0, 0,
			d3d11Src->GetD3D11Buffer(), 0, &box);

	}
	void D3D11CommandBuffer::CopyTexture(ITexture* src, ITexture* dst)
	{
		if (!src)
		{
			OutputDebugStringA("Warning: CopyTexture called with null source texture.\n");
			return;
		}
		if (!dst)
		{
			OutputDebugStringA("Warning: CopyTexture called with null destination texture.\n");
			return;
		}

		auto* d3d11Src = static_cast<D3D11Texture*>(src);
		auto* d3d11Dst = static_cast<D3D11Texture*>(dst);
		m_Context->CopyResource(d3d11Dst->GetD3D11Texture2D(), d3d11Src->GetD3D11Texture2D());
	}
	void D3D11CommandBuffer::CopyBufferToTexture(IBuffer* src, ITexture* dst, uint32_t mipLevel, uint32_t arraySlice)
	{
		if (!src || !dst) return;
		OutputDebugStringA("Warning: CopyBufferToTexture not yet implemented for D3D11\n");

	}
	void D3D11CommandBuffer::CopyTextureToBuffer(ITexture* src, IBuffer* dst, uint32_t mipLevel, uint32_t arraySlice)
	{
		if (!src || !dst) return;
		OutputDebugStringA("Warning: CopyTextureToBuffer not yet implemented for D3D11\n");

	}
	void D3D11CommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		m_Context->Dispatch(groupCountX, groupCountY, groupCountZ);
	}
	void D3D11CommandBuffer::DispatchIndirect(IBuffer* argsBuffer, uint32_t offset)
	{
		if (!argsBuffer)
		{
			return;
		}
		auto* d3d11ArgsBuffer = static_cast<D3D11Buffer*>(argsBuffer);
		m_Context->DispatchIndirect(d3d11ArgsBuffer->GetD3D11Buffer(), offset);
	}
	void D3D11CommandBuffer::BeginQuery(IQuery* query, uint32_t index)
	{
		// TODO: Implement when D3D11Query exists
		OutputDebugStringA("Warning: BeginQuery not yet implemented\n");
	}

	void D3D11CommandBuffer::EndQuery(IQuery* query, uint32_t index)
	{
		// TODO: Implement when D3D11Query exists
		OutputDebugStringA("Warning: EndQuery not yet implemented\n");
	}

	void D3D11CommandBuffer::WriteTimestamp(IQuery* query, PipelineStage stage, uint32_t index)
	{
		// TODO: Implement when D3D11Query exists
		OutputDebugStringA("Warning: WriteTimestamp not yet implemented\n");
	}

	void D3D11CommandBuffer::PushConstants(const void* data, uint32_t size, uint32_t offset)
	{
		// D3D11 doesn't have push constants
		// Would need to emulate with a small constant buffer
		OutputDebugStringA("Warning: PushConstants not natively supported in D3D11 (use constant buffers)\n");
	}
}