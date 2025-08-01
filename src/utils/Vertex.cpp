#include "Vertex.h"

//Vertex::Vertex(float x, float y, float z, float nx, float ny, float nz, float u, float v)
//	:
//	pos(x, y, z),
//	normals(nx,ny,nz),
//	texCoords(u,v)
//{

Vertex::Vertex()
{}

//}
Vertex::Vertex(float x, float y, float z,
	float u, float v,
	float nx, float ny, float nz,
	float tx, float ty, float tz,float tw)
	:
	pos(x,y,z),
	texCoord(u,v),
	normal(nx,ny,nz),
	tangent(tx, ty, tz,tw)
{
	
}


