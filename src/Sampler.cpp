#include "Sampler.h"

Sampler::Sampler(Graphics& g)
{
	D3D11_SAMPLER_DESC sp;
	ZeroMemory(&sp, sizeof(sp));

	sp.Filter = D3D11_FILTER_ANISOTROPIC;
	sp.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sp.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sp.MipLODBias = 0.0f;
	sp.MaxAnisotropy = 16;
	sp.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sp.MinLOD = 0;
	sp.MaxLOD = D3D11_FLOAT32_MAX;

	g.GetDevice()->CreateSamplerState(&sp, samplerState.GetAddressOf());
}

void Sampler::Bind(Graphics& g)
{
	g.GetContext()->PSSetSamplers(0, 1, samplerState.GetAddressOf());
}
