#pragma once
#include "utils/Vertex.h"
#include <wrl.h>
#include "utils/Mesh.h"

class Sphere
{
public:
	Sphere(const int SPHERE_RESOLUTION);
	Sphere(const int resolution, int slot);
	Sphere(const char* name,int res);
	~Sphere();
	std::shared_ptr<MeshResource> getMeshResource()const;


private:
	Vertex vertex;
	std::shared_ptr<MeshResource> m_Resource;
	const int SPHERE_RESOLUTION;
	int NumSphereVertices;
	int NumSphereFaces;
	XMMATRIX Rotationx;
	XMMATRIX Rotationy;


};

