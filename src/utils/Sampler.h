#pragma once
#include "Graphics.h"
#include <wrl.h>

class Sampler
{
public:
	Sampler(Graphics& g);
	~Sampler();
	void Bind(Graphics& g);
private:
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
};

