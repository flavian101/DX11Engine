#include "Mesh.h"

Mesh::Mesh(Graphics& g, const unsigned short indices[], Vertex v[], UINT indexCount)
	:
	indexCount(indexCount)
{

	//indexCount = sizeof(indices) / sizeof(indices[0]);
	IndexBuffer id(g, indices);
	id.Bind(g);


	VertexBuffer vb(g, v, sizeof(v), sizeof(Vertex));
	vb.Bind(g);

	VertexShader vs(g, L"assets/shaders/vs.hlsl");
	auto bytecode = vs.GetByteCode();
	vs.Bind(g);
	PixelShader ps(g, L"assets/shaders/ps.hlsl");

	ps.Bind(g);

	InputLayout layout(g, bytecode);
	layout.Bind(g);

	Topology tp(g, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	tp.Bind(g);
	
}

void Mesh::Draw(Graphics& g)
{
	g.Draw(indexCount);
}
