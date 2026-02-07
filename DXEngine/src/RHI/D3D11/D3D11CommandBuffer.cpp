#include "dxpch.h"
#include "D3D11CommandBuffer.h"
#include "D3D11Pipeline.h"
#include "D3D11Shader.h"
#include "D3D11Buffer.h"
#include "D3D11Texture.h"

namespace DXEngine::RHI
{
	D3D11CommandBuffer::D3D11CommandBuffer(ID3D11DeviceContext* context)
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
			m_Context->GSSetShader(static_cast<ID3D11GeometryShader*>(gs->GetD3D11Shader()), nullptr,0);
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
			m_Context->VSSetConstantBuffers(slot, 1, &cb);
			break;
		case ShaderStage::Pixel:
			m_Context->PSSetConstantBuffers(slot, 1, &cb);
			break;
			//more stages
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
	void D3D11CommandBuffer::SetRenderTarget(ITexture* colorTarget, ITexture* depthTarget)
	{
		ID3D11RenderTargetView* rtv = colorTarget ? static_cast<D3D11Texture*>(colorTarget)->GetRTV() : nullptr;

		ID3D11DepthStencilView* dsv = depthTarget ?
			static_cast<D3D11Texture*>(depthTarget)->GetDSV() : nullptr;

		m_Context->OMSetRenderTargets(1, &rtv, dsv);
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
}