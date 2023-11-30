#pragma once
#include "Graphics.h"
#include <wrl.h>
class Topology
{
	public:
	Topology(Graphics& g, D3D11_PRIMITIVE_TOPOLOGY type);
	void Bind(Graphics& g);

private:
	D3D11_PRIMITIVE_TOPOLOGY type;

};

