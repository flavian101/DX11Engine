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
#include "Sampler.h"
#include <vector>

class Mesh
{
public:
	Mesh(Graphics& g,const std::vector<unsigned short>& indices,const std::vector< Vertex>& v,
		LPCWSTR vertexShader, LPCWSTR pixelShader);
	~Mesh();

	void Draw(Graphics& g, XMMATRIX model, XMVECTOR camPos, XMVECTOR camTarget);
	void DrawSky(Graphics& g, XMMATRIX model);

	void UpdateMesh(Graphics& g);
	void UpdateLight(Graphics& g);
	void UpdateCamera(Graphics& g);
private:
	UINT indexCount;
	//constant buffer
	ConstantBuffer<TransfomBufferData> vsBuffer;
	VertexBuffer vb;
	IndexBuffer id;
	VertexShader vs;
	PixelShader ps;
	InputLayout layout;
	Topology tp;
	Sampler samp;

public:

	
};

