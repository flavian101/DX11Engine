#include "Vertex.h"

Vertex::Vertex(float x, float y, float z, float nx, float ny, float nz, float u, float v)
	:
	pos(x, y, z),
	normals(nx,ny,nz),
	texCoords(u,v)
{}
