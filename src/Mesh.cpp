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

	//psBuffer.data.light.spotPos.x = XMVectorGetX(camPos);
	//psBuffer.data.light.spotPos.y = XMVectorGetY(camPos);
	//psBuffer.data.light.spotPos.z = XMVectorGetZ(camPos);
	//
	////light dir
	//psBuffer.data.light.dir.x = XMVectorGetX(camTarget) - psBuffer.data.light.spotPos.x;
	//psBuffer.data.light.dir.y = XMVectorGetY(camTarget) - psBuffer.data.light.spotPos.y;
	//psBuffer.data.light.dir.z = XMVectorGetZ(camTarget) - psBuffer.data.light.spotPos.z;

	

	vsBuffer.data.WVP = XMMatrixTranspose(modelMatrix * g.GetCamera().GetView() * g.GetCamera().GetProjection());
	vsBuffer.data.Model = XMMatrixTranspose(modelMatrix);
	vsBuffer.Update(g);
	g.GetContext()->VSSetConstantBuffers(BindSlot::CB_Transform, 1, vsBuffer.GetAddressOf());
	
	
	g.Draw(indexCount);
}

void Mesh::DrawSky(Graphics& g, XMMATRIX model)
{
	id.Bind(g);
	vb.Bind(g);

	vs.Bind(g);
	ps.Bind(g);
	vsBuffer.data.WVP = XMMatrixTranspose(model * g.GetCamera().GetView() * g.GetCamera().GetProjection());
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

