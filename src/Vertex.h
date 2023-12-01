#pragma once
#include <vector>
#include <DirectXMath.h>

using namespace DirectX;
class Vertex
{
public:
	Vertex();
	//Vertex(float x, float y,float z,
	//float nx, float ny, float nz,
	//	float u, float v);
	Vertex(float x, float y, float z,
		float cr, float cg, float cb, float ca);

	//int getCount();
	//std::vector<Vertex> GetVertices();
	//void AddVertex(const Vertex& v);
	XMFLOAT3 pos;
	XMFLOAT4 color;

private:
	

private:
	//std::vector<Vertex> vertices;
	
	//XMFLOAT3 normals;
	//XMFLOAT2 texCoords;

};

