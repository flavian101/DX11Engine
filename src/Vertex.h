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
		float u, float v);

	
	XMFLOAT3 pos;
	XMFLOAT2 texCoord;

private:
	

private:
	
	//XMFLOAT3 normals;
	//XMFLOAT2 texCoords;

};

