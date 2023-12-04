#pragma once
#include "Vertex.h"
#include <wrl.h>

class Sphere
{
public:
	Sphere(const int SPHERE_RESOLUTION);
	Vertex* getVertex();
	const unsigned short* getIndices();
private:
	Vertex vertex;
	const int SPHERE_RESOLUTION;
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
};

