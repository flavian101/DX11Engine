#pragma once
#include "RHI/GraphicsDevice.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace DXEngine::RHI
{
	using Microsoft::WRL::ComPtr;

	class D3D11Pipeline : public IPipeline
	{
	public:
		D3D11Pipeline(ID3D11Device* device, const PipelineDesc& desc);
		~D3D11Pipeline() override = default;

		// IGraphicsResource
		const std::string& GetDebugName() const override { return m_DebugName; }
		void SetDebugName(const std::string& name) override { m_DebugName = name; }
		uint64_t GetGPUHandle() const override {
			return reinterpret_cast<uint64_t>(m_InputLayout.Get());
		}
		size_t GetMemoryUsage() const override { return sizeof(*this); }
		bool IsValid() const override { return m_InputLayout != nullptr; }

		// IPipeline
		const PipelineDesc& GetDesc() const override { return m_Desc; }
		void* GetNativeHandle() const override { return m_InputLayout.Get(); }

		// D3D11-specific state objects
		ID3D11InputLayout* GetInputLayout() const { return m_InputLayout.Get(); }
		ID3D11RasterizerState* GetRasterizerState() const { return m_RasterizerState.Get(); }
		ID3D11DepthStencilState* GetDepthStencilState() const { return m_DepthStencilState.Get(); }
		ID3D11BlendState* GetBlendState() const { return m_BlendState.Get(); }
		D3D11_PRIMITIVE_TOPOLOGY GetTopology() const { return m_Topology; }

		// Helper methods
		void Apply(ID3D11DeviceContext* context) const;

	private:
		bool CreateInputLayout(ID3D11Device* device, const PipelineDesc& desc);
		bool CreateRasterizerState(ID3D11Device* device, const PipelineDesc& desc);
		bool CreateDepthStencilState(ID3D11Device* device, const PipelineDesc& desc);
		bool CreateBlendState(ID3D11Device* device, const PipelineDesc& desc);

		D3D11_PRIMITIVE_TOPOLOGY GetD3DTopology(PrimitiveTopology topology) const;
		D3D11_CULL_MODE GetD3DCullMode(CullMode mode) const;
		D3D11_COMPARISON_FUNC GetD3DComparisonFunc(DepthTestMode mode) const;
		D3D11_BLEND GetD3DBlendFactor(BlendMode mode, bool isSrc) const;

		ComPtr<ID3D11InputLayout> m_InputLayout;
		ComPtr<ID3D11RasterizerState> m_RasterizerState;
		ComPtr<ID3D11DepthStencilState> m_DepthStencilState;
		ComPtr<ID3D11BlendState> m_BlendState;

		PipelineDesc m_Desc;
		D3D11_PRIMITIVE_TOPOLOGY m_Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		std::string m_DebugName;
	};

}