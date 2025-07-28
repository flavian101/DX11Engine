#pragma once
#include "Vertex.h"
#include <wrl.h>

class Sphere
{
public:
	Sphere(const int SPHERE_RESOLUTION);
	Sphere(const int resolution, int slot);
	Sphere(const char* name,int res);
	~Sphere();
	const std::vector<Vertex>& GetVertices() const;
	const std::vector<unsigned short>& GetIndices() const;

private:
	Vertex vertex;
	const int SPHERE_RESOLUTION;
	std::vector<Vertex> vertices;
	std::vector<unsigned short> indices;
	int NumSphereVertices;
	int NumSphereFaces;
	XMMATRIX Rotationx;
	XMMATRIX Rotationy;


};

