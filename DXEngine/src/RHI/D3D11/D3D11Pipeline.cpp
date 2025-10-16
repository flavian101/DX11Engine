#include "dxpch.h"
#include "D3D11Pipeline.h"
#include "D3D11Shader.h"

namespace DXEngine::RHI
{
	static DXGI_FORMAT GetDXGIFormat(DataFormat format) {
		switch (format) {
		case DataFormat::Float: return DXGI_FORMAT_R32_FLOAT;
		case DataFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
		case DataFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
		case DataFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case DataFormat::Int: return DXGI_FORMAT_R32_SINT;
		case DataFormat::Int2: return DXGI_FORMAT_R32G32_SINT;
		case DataFormat::Int3: return DXGI_FORMAT_R32G32B32_SINT;
		case DataFormat::Int4: return DXGI_FORMAT_R32G32B32A32_SINT;
		case DataFormat::UByte4: return DXGI_FORMAT_R8G8B8A8_UINT;
		case DataFormat::UByte4N: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case DataFormat::Short2: return DXGI_FORMAT_R16G16_SINT;
		case DataFormat::Short2N: return DXGI_FORMAT_R16G16_SNORM;
		case DataFormat::Short4: return DXGI_FORMAT_R16G16B16A16_SINT;
		case DataFormat::Short4N: return DXGI_FORMAT_R16G16B16A16_SNORM;
		case DataFormat::Half2: return DXGI_FORMAT_R16G16_FLOAT;
		case DataFormat::Half4: return DXGI_FORMAT_R16G16B16A16_FLOAT;
		default: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		}
	}

	D3D11Pipeline::D3D11Pipeline(ID3D11Device* device, const PipelineDesc& desc)
		:
		m_Desc(desc),
		m_DebugName(desc.debugName)
	{
		if (!device) {
			throw std::runtime_error("D3D11Pipeline: Device cannot be null");
		}

		if (!desc.vertexShader || !desc.pixelShader) {
			throw std::runtime_error("D3D11Pipeline: Vertex and pixel shaders are required");
		}
		//create input layout
		if (!CreateInputLayout(device, desc))
		{
			throw std::runtime_error("D3D11Pipeline: Failed to create input layout");
		}

		// Create rasterizer state
		if (!CreateRasterizerState(device, desc)) {
			throw std::runtime_error("D3D11Pipeline: Failed to create rasterizer state");
		}

		// Create depth-stencil state
		if (!CreateDepthStencilState(device, desc)) {
			throw std::runtime_error("D3D11Pipeline: Failed to create depth-stencil state");
		}

		// Create blend state
		if (!CreateBlendState(device, desc)) {
			throw std::runtime_error("D3D11Pipeline: Failed to create blend state");
		}

		// Set topology
		m_Topology = GetD3DTopology(desc.topology);

		// Set debug name
		if (!m_DebugName.empty()) {
			SetDebugName(m_DebugName);
		}
	}
	void D3D11Pipeline::Apply(ID3D11DeviceContext* context) const
	{
	}
	bool D3D11Pipeline::CreateInputLayout(ID3D11Device* device, const PipelineDesc& desc)
	{
		if (desc.inputLayout.empty())
		{
			return true;
		}

		std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
		elements.reserve(desc.inputLayout.size());

		for (const auto& element : desc.inputLayout)
		{
			D3D11_INPUT_ELEMENT_DESC desc = {};
			desc.SemanticName = element.SemanticName.c_str();
			desc.SemanticIndex = element.SemanticIndex;
			desc.Format = GetDXGIFormat(element.Format);
			desc.InputSlot = element.Slot;
			desc.AlignedByteOffset = element.Offset;
			desc.InputSlotClass = element.PerInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
			desc.InstanceDataStepRate = element.PerInstance ? 1 : 0;
			elements.push_back(desc);
		}
	 
		// Get shader bytecode
		auto* vsShader = static_cast<D3D11Shader*>(desc.vertexShader);
		if (!vsShader || !vsShader->GetBytecode()) {
			OutputDebugStringA("D3D11Pipeline: Vertex shader bytecode is invalid\n");
			return false;
		}

		HRESULT hr = device->CreateInputLayout(
			elements.data(),
			static_cast<UINT>(elements.size()),
			vsShader->GetBytecode(),
			vsShader->GetBytecodeSize(),
			m_InputLayout.GetAddressOf()
		);

		return SUCCEEDED(hr);
	}
	bool D3D11Pipeline::CreateRasterizerState(ID3D11Device* device, const PipelineDesc& desc)
	{
		D3D11_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = desc.rasterizer.wireframe? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = GetD3DCullMode(desc.rasterizer.cullMode);
		rasterizerDesc.FrontCounterClockwise = static_cast<BOOL>(desc.rasterizer.frontCounterClockwise);
		rasterizerDesc.DepthBias = 0;
		rasterizerDesc.DepthBiasClamp = 0.0f;
		rasterizerDesc.SlopeScaledDepthBias = 0.0f;
		rasterizerDesc.DepthClipEnable = TRUE;
		rasterizerDesc.ScissorEnable = FALSE;
		rasterizerDesc.MultisampleEnable = FALSE;
		rasterizerDesc.AntialiasedLineEnable = FALSE;

		HRESULT hr = device->CreateRasterizerState(&rasterizerDesc, m_RasterizerState.GetAddressOf());
		return SUCCEEDED(hr);
	}
	bool D3D11Pipeline::CreateDepthStencilState(ID3D11Device* device, const PipelineDesc& desc)
	{
		//The stencil is Off So improvement of Description is required to turn on On request 
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = (desc.depthTest != DepthTestMode::None);
		depthStencilDesc.DepthWriteMask = desc.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = GetD3DComparisonFunc(desc.depthTest);

		depthStencilDesc.StencilEnable = FALSE;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;

		// Front face
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		// Back face
		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		HRESULT hr = device->CreateDepthStencilState(&depthStencilDesc, m_DepthStencilState.GetAddressOf());
		return SUCCEEDED(hr);
	}
	bool D3D11Pipeline::CreateBlendState(ID3D11Device* device, const PipelineDesc& desc)
	{
		//To add Support for Multiple RenderTargets later for (particles,lighting,decals,and post-processing and others)
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;

		D3D11_RENDER_TARGET_BLEND_DESC& rtBlendDesc = blendDesc.RenderTarget[0];

		if (desc.blendMode == BlendMode::Opaque) {
			rtBlendDesc.BlendEnable = FALSE;
			rtBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
		else if (desc.blendMode == BlendMode::AlphaBlend) {
			rtBlendDesc.BlendEnable = TRUE;
			rtBlendDesc.SrcBlend = D3D11_BLEND_SRC_ALPHA;
			rtBlendDesc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			rtBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
			rtBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
			rtBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
			rtBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			rtBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
		else if (desc.blendMode == BlendMode::Additive) {
			rtBlendDesc.BlendEnable = TRUE;
			rtBlendDesc.SrcBlend = D3D11_BLEND_ONE;
			rtBlendDesc.DestBlend = D3D11_BLEND_ONE;
			rtBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
			rtBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
			rtBlendDesc.DestBlendAlpha = D3D11_BLEND_ONE;
			rtBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			rtBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
		else if (desc.blendMode == BlendMode::Multiply) {
			rtBlendDesc.BlendEnable = TRUE;
			rtBlendDesc.SrcBlend = D3D11_BLEND_ZERO;
			rtBlendDesc.DestBlend = D3D11_BLEND_SRC_COLOR;
			rtBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
			rtBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
			rtBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
			rtBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			rtBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}

		HRESULT hr = device->CreateBlendState(&blendDesc, m_BlendState.GetAddressOf());
		return SUCCEEDED(hr);
	}
	D3D11_PRIMITIVE_TOPOLOGY D3D11Pipeline::GetD3DTopology(PrimitiveTopology topology) const
	{
		switch (topology) {
		case PrimitiveTopology::TriangleList:       return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case PrimitiveTopology::TriangleStrip:      return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case PrimitiveTopology::LineList:           return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		case PrimitiveTopology::LineStrip:          return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case PrimitiveTopology::PointList:          return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		case PrimitiveTopology::TriangleListAdj:    return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
		case PrimitiveTopology::TriangleStripAdj:   return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
		case PrimitiveTopology::LineListAdj:        return D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
		case PrimitiveTopology::LineStripAdj:       return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
		default:                                     return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}
	D3D11_CULL_MODE D3D11Pipeline::GetD3DCullMode(CullMode mode) const
	{
		switch (mode) {
		case CullMode::None: return D3D11_CULL_NONE;
		case CullMode::Front: return D3D11_CULL_FRONT;
		case CullMode::Back: return D3D11_CULL_BACK;
		default: return D3D11_CULL_BACK;
		}
	}
	D3D11_COMPARISON_FUNC D3D11Pipeline::GetD3DComparisonFunc(DepthTestMode mode) const
	{
		switch (mode) {
		case DepthTestMode::None: return D3D11_COMPARISON_ALWAYS;
		case DepthTestMode::Less: return D3D11_COMPARISON_LESS;
		case DepthTestMode::LessEqual: return D3D11_COMPARISON_LESS_EQUAL;
		case DepthTestMode::Greater: return D3D11_COMPARISON_GREATER;
		case DepthTestMode::Equal: return D3D11_COMPARISON_EQUAL;
		default: return D3D11_COMPARISON_LESS;
		}
	}
	D3D11_BLEND D3D11Pipeline::GetD3DBlendFactor(BlendMode mode, bool isSrc) const
	{
		switch (mode) {
		case BlendMode::Opaque:
			return D3D11_BLEND_ONE;

		case BlendMode::AlphaBlend:
			return isSrc ? D3D11_BLEND_SRC_ALPHA : D3D11_BLEND_INV_SRC_ALPHA;

		case BlendMode::Additive:
			return D3D11_BLEND_ONE;

		case BlendMode::Multiply:
			return isSrc ? D3D11_BLEND_ZERO : D3D11_BLEND_SRC_COLOR;

		default:
			return D3D11_BLEND_ONE;
		}
	}

	
}