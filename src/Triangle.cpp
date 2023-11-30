#include "Triangle.h"

Triangle::Triangle(Graphics& g)
{
	Vertex v[] =
	{
		Vertex(0.0f, 0.5f, 0.5f),
		Vertex(0.5f, -0.5f, 0.5f),
		Vertex(-0.5f, -0.5f, 0.5f)
	};
	//v->AddVertex(*v);

	VertexBuffer vb(g, v, sizeof(v), sizeof(Vertex));
	vb.Bind(g);

	VertexShader vs(g,L"assets/shaders/vs.hlsl");
	auto bytecode = vs.GetByteCode();
	vs.Bind(g);
	PixelShader ps(g, L"assets/shaders/ps.hlsl");
	
	ps.Bind(g);

	InputLayout layout(g, bytecode);
	layout.Bind(g);

	Topology tp(g, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	tp.Bind(g);

}

void Triangle::Draw(Graphics& g)
{
	g.Draw(3);
}
