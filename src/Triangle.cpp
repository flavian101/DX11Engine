#include "Triangle.h"

Triangle::Triangle(Graphics& g)
{
	Vertex v[] =
	{
		Vertex(-0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),
		Vertex(-0.5f,  0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f),
		Vertex(0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f),
		Vertex(0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f),
	};


	const unsigned short indices[] =
	{
		 0, 1, 2,
		0, 2,3
	};

	indexCount = sizeof(indices) / sizeof(indices[0]);
	IndexBuffer id(g, indices);
	id.Bind(g);
	

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
	g.Draw(indexCount);
}
