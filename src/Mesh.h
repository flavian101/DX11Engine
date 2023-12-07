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
	Mesh(Graphics& g, std::vector<unsigned short> indices,std::vector< Vertex> v,
		LPCWSTR vertexShader, LPCWSTR pixelShader);

	void Draw(Graphics& g, XMMATRIX model, XMVECTOR camPos, XMVECTOR camTarget);
	void DrawSky(Graphics& g, XMMATRIX model, XMVECTOR camPos);

	void UpdateMesh(Graphics& g);
	void UpdateLight(Graphics& g);
	void UpdateCamera(Graphics& g);
private:
	UINT indexCount;
	//constant buffer
	ConstantBuffer<cb_psConstantBuffer> psBuffer;
	ConstantBuffer<cb_vsConstantBuffer> vsBuffer;

	Sampler samp;
	//matrices
	XMMATRIX WVP;
	XMMATRIX Model;
public:

	
};

