#pragma once
#include "renderer/RendererCommand.h"
#include <wrl.h>

namespace DXEngine {

	class Topology
	{
	public:
		Topology( D3D11_PRIMITIVE_TOPOLOGY type);
		~Topology();
		void Bind();

	private:
		D3D11_PRIMITIVE_TOPOLOGY type;

	};

}