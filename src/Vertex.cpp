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
Vertex::Vertex(float x, float y, float z)
	:
	pos(x,y,z)
{
	//AddVertex();
	//vertices.push_back(Vertex(x, y, z));
}

//int Vertex::getCount()
//{
//	return vertices.size();
//}
//
//std::vector<Vertex> Vertex::GetVertices()
//{
//	return vertices;
//}
//
//void Vertex::AddVertex(const Vertex& v)
//{
//	vertices.push_back(v);
//}

