#pragma once
#include <vector>
#include <DirectXMath.h>

using namespace DirectX;
class Vertex
{
public:
	Vertex(float x, float y,float z,
	float nx, float ny, float nz,
		float u, float v);

private:
	
	XMFLOAT3 pos;
	XMFLOAT3 normals;
	XMFLOAT2 texCoords;

};

