#pragma once
#include "Vertex.h"
#include "Graphics.h"
#include "VertexBuffer.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "InputLayout.h"
#include "Topology.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"

class Mesh
{
public:
	Mesh(Graphics& g, const unsigned short indices[], Vertex v[],UINT indexCount);

	void Draw(Graphics& g, FXMMATRIX model);
	void UpdateMesh(Graphics& g);
private:
	UINT indexCount;
	//constant buffer
	ConstantBuffer<cb_vsConstantBuffer> vsBuffer;
	//matrices
	XMMATRIX WVP;
	XMMATRIX World;
	XMMATRIX Model;
public:

	
};

