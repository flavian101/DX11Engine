#pragma once
#include "Graphics.h"
#include <wrl.h>

namespace DXEngine {

	class Topology
	{
	public:
		Topology(Graphics& g, D3D11_PRIMITIVE_TOPOLOGY type);
		~Topology();
		void Bind(Graphics& g);

	private:
		D3D11_PRIMITIVE_TOPOLOGY type;

	};

}