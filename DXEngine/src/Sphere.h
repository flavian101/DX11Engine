#pragma once
#include "utils/Vertex.h"
#include "utils/Mesh.h"


namespace DXEngine {



	class Sphere
	{
	public:
		Sphere(const int SPHERE_RESOLUTION);
		Sphere(const int resolution, int slot);
		Sphere(const char* name, int res);
		~Sphere();
		std::shared_ptr<MeshResource> getMeshResource()const;

	private:
		Vertex vertex;
		std::shared_ptr<MeshResource> m_Resource;
		const int SPHERE_RESOLUTION;
		int NumSphereVertices;
		int NumSphereFaces;
		DirectX::XMMATRIX Rotationx;
		DirectX::XMMATRIX Rotationy;


	};
}
