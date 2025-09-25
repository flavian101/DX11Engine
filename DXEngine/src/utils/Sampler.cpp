#include "dxpch.h"
#include "Sampler.h"

namespace DXEngine {
	void SamplerManager::Initilize()
	{
		CreateStandardSampler();
		CreateShadowSamplers();
		CreateFilteringSamplers();
	}
	void SamplerManager::Shutdown()
	{
		m_StandardSampler.Reset();
		m_PointSampler.Reset();
		m_LinearSampler.Reset();
		m_AnisotropicSampler.Reset();
		m_ShadowSampler.Reset();
		m_ShadowPCFSampler.Reset();
	}
	Microsoft::WRL::ComPtr<ID3D11SamplerState> SamplerManager::CreateSampler(const SamplerDesc& desc)
	{
		D3D11_SAMPLER_DESC d3dDesc = {};

		// Set filter based on sampler type
		switch (desc.type) {
		case SamplerType::Point:
			d3dDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			break;
		case SamplerType::Linear:
			d3dDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			break;
		case SamplerType::Anisotropic:
			d3dDesc.Filter = D3D11_FILTER_ANISOTROPIC;
			break;
		case SamplerType::Shadow:
			d3dDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			break;
		case SamplerType::ShadowPCF:
			d3dDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
			break;
		}

		d3dDesc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.addressU);
		d3dDesc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.addressV);
		d3dDesc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.addressW);

		memcpy(d3dDesc.BorderColor, desc.borderColor, sizeof(float) * 4);

		d3dDesc.MaxAnisotropy = desc.maxAnisotropy;
		d3dDesc.MipLODBias = desc.mipLODBias;
		d3dDesc.MinLOD = desc.minLOD;
		d3dDesc.MaxLOD = desc.maxLOD;
		d3dDesc.ComparisonFunc = desc.comparisonFunc;

		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
		HRESULT hr = RenderCommand::GetDevice()->CreateSamplerState(&d3dDesc, &samplerState);

		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create custom sampler state\n");
			return nullptr;
		}

		return samplerState;
	}
	void SamplerManager::BindPixelShaderSamplers()
	{
		ID3D11SamplerState* samplers[] = {
		   m_StandardSampler.Get(),    // s0 - standardSampler
		   m_ShadowSampler.Get()       // s1 - shadowSampler
		};

		RenderCommand::GetContext()->PSSetSamplers(0, 2, samplers);

	}
	void SamplerManager::BindVertexShaderSamplers()
	{
		// Vertex shaders might need samplers for vertex texture fetch
		ID3D11SamplerState* samplers[] = {
			m_StandardSampler.Get(),
			m_ShadowSampler.Get()
		};

		RenderCommand::GetContext()->VSSetSamplers(0, 2, samplers);

	}
	void SamplerManager::CreateStandardSampler()
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.BorderColor[0] = 0.0f;
		samplerDesc.BorderColor[1] = 0.0f;
		samplerDesc.BorderColor[2] = 0.0f;
		samplerDesc.BorderColor[3] = 0.0f;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		HRESULT hr = RenderCommand::GetDevice()->CreateSamplerState(&samplerDesc, &m_StandardSampler);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create standard sampler\n");
		}
	}
	void SamplerManager::CreateShadowSamplers()
	{
		D3D11_SAMPLER_DESC shadowDesc = {};
		shadowDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		shadowDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		shadowDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		shadowDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		shadowDesc.MipLODBias = 0.0f;
		shadowDesc.MaxAnisotropy = 1;
		shadowDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

		// White border color - areas outside shadow map are fully lit
		shadowDesc.BorderColor[0] = 1.0f;
		shadowDesc.BorderColor[1] = 1.0f;
		shadowDesc.BorderColor[2] = 1.0f;
		shadowDesc.BorderColor[3] = 1.0f;

		shadowDesc.MinLOD = 0.0f;
		shadowDesc.MaxLOD = 0.0f; // No mipmaps for shadow maps

		HRESULT hr = RenderCommand::GetDevice()->CreateSamplerState(&shadowDesc, &m_ShadowSampler);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create shadow comparison sampler\n");
		}

		// PCF (Percentage Closer Filtering) shadow sampler
		D3D11_SAMPLER_DESC pcfDesc = shadowDesc;
		pcfDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // Better filtering for PCF

		hr = RenderCommand::GetDevice()->CreateSamplerState(&pcfDesc, &m_ShadowPCFSampler);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create PCF shadow sampler\n");
		}

	}
	void SamplerManager::CreateFilteringSamplers()
	{
		// Point sampler
		D3D11_SAMPLER_DESC pointDesc = {};
		pointDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		pointDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		pointDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		pointDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		pointDesc.MipLODBias = 0.0f;
		pointDesc.MaxAnisotropy = 1;
		pointDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		pointDesc.MinLOD = 0.0f;
		pointDesc.MaxLOD = D3D11_FLOAT32_MAX;

		HRESULT hr = RenderCommand::GetDevice()->CreateSamplerState(&pointDesc, &m_PointSampler);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create point sampler\n");
		}

		// Linear sampler
		D3D11_SAMPLER_DESC linearDesc = {};
		linearDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		linearDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		linearDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		linearDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		linearDesc.MipLODBias = 0.0f;
		linearDesc.MaxAnisotropy = 1;
		linearDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		linearDesc.MinLOD = 0.0f;
		linearDesc.MaxLOD = D3D11_FLOAT32_MAX;

		hr = RenderCommand::GetDevice()->CreateSamplerState(&linearDesc, &m_LinearSampler);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create linear sampler\n");
		}

		// Anisotropic sampler
		D3D11_SAMPLER_DESC anisoDesc = {};
		anisoDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		anisoDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		anisoDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		anisoDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		anisoDesc.MipLODBias = 0.0f;
		anisoDesc.MaxAnisotropy = 16;
		anisoDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		anisoDesc.MinLOD = 0.0f;
		anisoDesc.MaxLOD = D3D11_FLOAT32_MAX;

		hr = RenderCommand::GetDevice()->CreateSamplerState(&anisoDesc, &m_AnisotropicSampler);
		if (FAILED(hr)) {
			OutputDebugStringA("Failed to create anisotropic sampler\n");
		}
	}
	Sampler::Sampler(SamplerType type, SamplerAddressMode addressMode)
	{
		SamplerDesc desc;
		desc.type = type;
		desc.addressU = addressMode;
		desc.addressV = addressMode;
		desc.addressW = addressMode;

		Initialized(desc);
	}
	Sampler::~Sampler()
	{
	}
	bool Sampler::Initialized(const SamplerDesc& desc)
	{
		m_Description = desc;
		m_SamplerState = SamplerManager::Instance().CreateSampler(desc);
		return m_SamplerState != nullptr;
	}
	void Sampler::Bind(uint32_t slot, bool pixelShader, bool vertexShader)
	{
		if (!m_SamplerState) return;

		ID3D11SamplerState* sampler = m_SamplerState.Get();

		if (pixelShader) {
			RenderCommand::GetContext()->PSSetSamplers(slot, 1, &sampler);
		}

		if (vertexShader) {
			RenderCommand::GetContext()->VSSetSamplers(slot, 1, &sampler);
		}
	}
	void Sampler::Release()
	{
		m_SamplerState.Reset();
	}
}