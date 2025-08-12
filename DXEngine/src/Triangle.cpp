#include "dxpch.h"
#include "Triangle.h"
#include "utils/material/Material.h"
#include <utils/Vertex.h>
#include <utils/Mesh.h>
#include "utils/Texture.h"
#include <utils/Mesh/VertexAttribute.h>
#include <utils/Mesh/MeshResource.h>

namespace DXEngine {


	Triangle::Triangle()
		:
		Model()
	{
		Initialize();
		auto grassMaterial = MaterialFactory::CreateTexturedNormalMaterial("Ground Material");
		auto grassDiffuse = std::make_shared<Texture>("assets/textures/grass.jpg");
		auto grassNormal = std::make_shared<Texture>("assets/textures/grassNormal.jpg");
		grassMaterial->SetDiffuseTexture(grassDiffuse);
		grassMaterial->SetNormalTexture(grassNormal);
		grassMaterial->SetTextureScale({ 100.0f, 100.0f });

		m_Mesh->SetMaterial(grassMaterial);
	}
	void Triangle::Initialize()
	{
		std::vector<Vertex> vertices;
		std::vector<unsigned short> ind;

		DirectX::XMFLOAT4 tangent(1.0f, 0.0f, 0.0f, 1.0f);


		vertices.push_back(Vertex(-1.0f, 0.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));
		vertices.push_back(Vertex( 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));
		vertices.push_back(Vertex( 1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));
		vertices.push_back(Vertex(-1.0f, 0.0f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, tangent.x, tangent.y, tangent.z, tangent.w));


		ind.push_back(0);
		ind.push_back(2);
		ind.push_back(1);
		ind.push_back(0);
		ind.push_back(3);
		ind.push_back(2);

		auto meshData = std::make_shared<MeshResource>(std::move(vertices), std::move(ind));
		m_Mesh = std::make_shared<Mesh>(meshData);

	}
}
