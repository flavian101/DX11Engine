#include "Topology.h"
namespace DXEngine {

	Topology::Topology( D3D11_PRIMITIVE_TOPOLOGY type)
		:
		type(type)
	{
	}

	Topology::~Topology()
	{
	}

	void Topology::Bind()
	{
		RenderCommand::GetContext()->IASetPrimitiveTopology(type);
	}
}