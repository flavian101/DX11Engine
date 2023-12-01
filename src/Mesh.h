#pragma once
#include "Vertex.h"
#include "Graphics.h"
#include "VertexBuffer.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "InputLayout.h"
#include "Topology.h"
#include "IndexBuffer.h"

class Mesh
{
public:
	Mesh(Graphics& g, const unsigned short indices[], Vertex v[],UINT indexCount);

	void Draw(Graphics& g);
private:
	UINT indexCount;
public:

	
};

