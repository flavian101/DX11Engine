#include "Topology.h"

Topology::Topology(Graphics& g, D3D11_PRIMITIVE_TOPOLOGY type)
	:
	type(type)
{}

Topology::~Topology()
{
}

void Topology::Bind(Graphics& g)
{
	g.GetContext()->IASetPrimitiveTopology(type);
}
