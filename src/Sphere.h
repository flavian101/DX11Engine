#pragma once
#include "Vertex.h"
#include <wrl.h>

class Sphere
{
public:
	Sphere(const int SPHERE_RESOLUTION);
	Sphere(const int resolution, int slot);
	std::vector<Vertex> getVertex();
	std::vector<unsigned short> getIndices();

private:
	Vertex vertex;
	const int SPHERE_RESOLUTION;
	std::vector<Vertex> vertices;
	std::vector<unsigned short> indices;
};

