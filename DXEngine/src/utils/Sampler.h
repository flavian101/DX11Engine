#pragma once
#include "renderer/RendererCommand.h"
#include <wrl.h>

namespace DXEngine {
	class Sampler
	{
	public:
		Sampler();
		~Sampler();
		void Bind();
	private:
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
	};
}
