#include "Mesh.h"

Mesh::Mesh(Graphics& g, const unsigned short indices[], Vertex v[], UINT indexCount,UINT size)
	:
	indexCount(indexCount),
	samp(g)
{

	//indexCount = sizeof(indices) / sizeof(indices[0]);
	IndexBuffer id(g, indices, indexCount);
	id.Bind(g);

	UINT count = size;

	VertexBuffer vb(g, v, count);
	vb.Bind(g);

	//initialize constant buffer
	vsBuffer.Initialize(g);
	
	

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


void Mesh::Draw(Graphics& g,FXMMATRIX modelMatrix)
{
	Model = modelMatrix;
	World = XMMatrixIdentity();

	WVP = Model * g.GetCamera() * g.GetProjection();
	vsBuffer.data.WVP = XMMatrixTranspose(WVP);
	vsBuffer.data.World = XMMatrixTranspose(Model);
	vsBuffer.Update(g);
	g.GetContext()->VSSetConstantBuffers(0,1,vsBuffer.GetAddressOf());
	samp.Bind(g);
	g.Draw(indexCount);
}

void Mesh::UpdateMesh(Graphics& g)
{

}

