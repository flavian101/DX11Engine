#include "Mesh.h"



Mesh::Mesh(Graphics& g,const std::vector<unsigned short>& indices, const std::vector< Vertex>& v,
	LPCWSTR vertexShader, LPCWSTR pixelShader)
	:
	vb(g, v),
	id(g, indices),
	vs(g, vertexShader),
	ps(g, pixelShader),
	layout(g, vs.GetByteCode()),
	indexCount(indices.size()),
	tp(g, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
	samp(g)
{

	//indexCount = sizeof(indices) / sizeof(indices[0]);
	samp.Bind(g);

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




	layout.Bind(g);
	tp.Bind(g);
}

Mesh::~Mesh()
{

}

void Mesh::Draw(Graphics& g,XMMATRIX modelMatrix, XMVECTOR camPos, XMVECTOR camTarget)
{
	id.Bind(g);
	vb.Bind(g);

	vs.Bind(g);
	ps.Bind(g);

	psBuffer.data.light.spotPos.x = XMVectorGetX(camPos);
	psBuffer.data.light.spotPos.y = XMVectorGetY(camPos);
	psBuffer.data.light.spotPos.z = XMVectorGetZ(camPos);

	//light dir
	psBuffer.data.light.dir.x = XMVectorGetX(camTarget) - psBuffer.data.light.spotPos.x;
	psBuffer.data.light.dir.y = XMVectorGetY(camTarget) - psBuffer.data.light.spotPos.y;
	psBuffer.data.light.dir.z = XMVectorGetZ(camTarget) - psBuffer.data.light.spotPos.z;

	psBuffer.Update(g);
	g.GetContext()->PSSetConstantBuffers(0, 1, psBuffer.GetAddressOf());
	

	vsBuffer.data.WVP = XMMatrixTranspose(modelMatrix * g.GetCamera() * g.GetProjection());
	vsBuffer.data.Model = XMMatrixTranspose(modelMatrix);
	vsBuffer.Update(g);
	g.GetContext()->VSSetConstantBuffers(0, 1, vsBuffer.GetAddressOf());
	


	
	g.Draw(indexCount);
}

void Mesh::DrawSky(Graphics& g, XMMATRIX model)
{
	id.Bind(g);
	vb.Bind(g);

	vs.Bind(g);
	ps.Bind(g);
	vsBuffer.data.WVP = XMMatrixTranspose(model * g.GetCamera() * g.GetProjection());
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

