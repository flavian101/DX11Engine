#include "Mesh.h"



Mesh::Mesh(Graphics& g, std::vector<unsigned short> indices, std::vector< Vertex> v,
	LPCWSTR vertexShader, LPCWSTR pixelShader)
	:
	indexCount(indices.size()),
	samp(g)
{

	//indexCount = sizeof(indices) / sizeof(indices[0]);
	IndexBuffer id(g, indices);
	id.Bind(g);


	VertexBuffer vb(g, v);
	vb.Bind(g);

	//initialize constant buffer
	psBuffer.Initialize(g);

	psBuffer.data.light.pointPos = XMFLOAT3(0.0f, 10.0f, 0.0f);
	psBuffer.data.light.spotPos = XMFLOAT3(0.0f, 1.0f, 0.0f);
	psBuffer.data.light.dir = XMFLOAT3(0.0f, 1.0f, 0.0f);
	psBuffer.data.light.range = 1000.0f;
	psBuffer.data.light.cone = 20.0f;
	psBuffer.data.light.att = XMFLOAT3(0.4f, 0.02f, 0.0f);
	psBuffer.data.light.ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	psBuffer.data.light.diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);

	vsBuffer.Initialize(g);


	VertexShader vs(g, vertexShader);
	auto bytecode = vs.GetByteCode();
	vs.Bind(g);
	PixelShader ps(g, pixelShader);

	ps.Bind(g);
	samp.Bind(g);

	InputLayout layout(g, bytecode);
	layout.Bind(g);

	Topology tp(g, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	tp.Bind(g);

	
	
}


void Mesh::Draw(Graphics& g,XMMATRIX modelMatrix, XMVECTOR camPos, XMVECTOR camTarget)
{

	
	psBuffer.data.light.spotPos.x = XMVectorGetX(camPos);
	psBuffer.data.light.spotPos.y = XMVectorGetY(camPos);
	psBuffer.data.light.spotPos.z = XMVectorGetZ(camPos);

	//light dir
	psBuffer.data.light.dir.x = XMVectorGetX(camTarget) - psBuffer.data.light.spotPos.x;
	psBuffer.data.light.dir.y = XMVectorGetY(camTarget) - psBuffer.data.light.spotPos.y;
	psBuffer.data.light.dir.z = XMVectorGetZ(camTarget) - psBuffer.data.light.spotPos.z;

	psBuffer.Update(g);
	g.GetContext()->PSSetConstantBuffers(0, 1, psBuffer.GetAddressOf());
	


	Model = modelMatrix;

	WVP = Model * g.GetCamera() * g.GetProjection();
	vsBuffer.data.WVP = XMMatrixTranspose(WVP);
	vsBuffer.data.Model = XMMatrixTranspose(Model);
	vsBuffer.Update(g);
	g.GetContext()->VSSetConstantBuffers(0, 1, vsBuffer.GetAddressOf());
	


	
	g.Draw(indexCount);
}

void Mesh::DrawSky(Graphics& g, XMMATRIX model)
{
	Model = model;

	WVP = model * g.GetCamera() * g.GetProjection();
	vsBuffer.data.WVP = XMMatrixTranspose(WVP);
	vsBuffer.data.Model = XMMatrixTranspose(model);
	vsBuffer.Update(g);

	g.GetContext()->VSSetConstantBuffers(0, 1, vsBuffer.GetAddressOf());


	g.DrawSkybox(indexCount);

	
}

void Mesh::UpdateMesh(Graphics& g)
{


}

void Mesh::UpdateLight(Graphics& g)
{
	
	
}

void Mesh::UpdateCamera(Graphics& g)
{
	
}

