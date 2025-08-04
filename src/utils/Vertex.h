#pragma once
#include <vector>
#include <DirectXMath.h>

namespace DXEngine {

	 ;
	class Vertex
	{
	public:
		Vertex();
		//Vertex(float x, float y,float z,
		//float nx, float ny, float nz,
		//	float u, float v);
		Vertex(float x, float y, float z,
			float u, float v,
			float nx, float ny, float nz,
			float tx, float ty, float tz, float tw);


		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 texCoord;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT4 tangent;
	private:


	private:

		//XMFLOAT3 normals;
		//XMFLOAT2 texCoords;

	};
}
