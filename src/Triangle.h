#pragma once
#include "Vertex.h"
#include "Graphics.h"
#include "VertexBuffer.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "InputLayout.h"
#include "Topology.h"
#include "IndexBuffer.h"



class Triangle
{
public:
	Triangle(Graphics& g);
	void Draw(Graphics& g);
private:
	UINT indexCount;
};

