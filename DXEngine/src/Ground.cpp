#include "dxpch.h"
#include "Ground.h"
#include "utils/material/Material.h"
#include <utils/mesh/Mesh.h>
#include "utils/Texture.h"
#include <utils/Mesh/Utils/VertexAttribute.h>
#include <utils/Mesh/Resource/MeshResource.h>

namespace DXEngine {


	Ground::Ground()
		:
		Model()
	{
		Initialize();
		

	}
	void Ground::Initialize()
	{
		auto layout = VertexLayout::CreateLit();
		auto vertexData = std::make_unique<VertexData>(layout);
		vertexData->Resize(4); // 4 vertices for a quad

        // Define ground vertices (large plane)
        float size = 1.0f; // Unit size, will be scaled by transform if needed
        DirectX::XMFLOAT3 positions[4] = {
            {-size, 0.0f, -size}, // Bottom-left
            { size, 0.0f, -size}, // Bottom-right  
            { size, 0.0f,  size}, // Top-right
            {-size, 0.0f,  size}  // Top-left
        };

        DirectX::XMFLOAT3 normal(0.0f, 1.0f, 0.0f); // Up normal
        DirectX::XMFLOAT4 tangent(1.0f, 0.0f, 0.0f, 1.0f); // Right tangent

        DirectX::XMFLOAT2 texCoords[4] = {
            {0.0f, 1.0f}, // Bottom-left
            {1.0f, 1.0f}, // Bottom-right
            {1.0f, 0.0f}, // Top-right
            {0.0f, 0.0f}  // Top-left
        };

        // Set vertex attributes
        for (int i = 0; i < 4; ++i)
        {
            vertexData->SetAttribute(i, VertexAttributeType::Position, positions[i]);
            vertexData->SetAttribute(i, VertexAttributeType::Normal, normal);
            vertexData->SetAttribute(i, VertexAttributeType::TexCoord0, texCoords[i]);
            vertexData->SetAttribute(i, VertexAttributeType::Tangent, tangent);
        }

        // Create index data
        auto indexData = std::make_unique<IndexData>(IndexType::UInt16);
        indexData->AddTriangle(0, 2, 1); // First triangle
        indexData->AddTriangle(0, 3, 2); // Second triangle

        // Create mesh resource
        auto meshResource = std::make_shared<MeshResource>("GroundMesh");
        meshResource->SetVertexData(std::move(vertexData));
        meshResource->SetIndexData(std::move(indexData));
        meshResource->SetTopology(PrimitiveTopology::TriangleList);

        // Generate bounds
        meshResource->GenerateBounds();

        // Create mesh
        auto mesh = std::make_shared<Mesh>(meshResource);

        auto grassMaterial = MaterialFactory::CreateLitMaterial("Ground Material");
        auto grassDiffuse = std::make_shared<Texture>("assets/textures/grass/grass_with_mud_and_stones_39_46_diffuse.jpg");
        auto grassNormal = std::make_shared<Texture>("assets/textures/grass/grass_with_mud_and_stones_39_46_normal.jpg");
        auto grassRoughness = std::make_shared<Texture>("assets/textures/grass/grass_with_mud_and_stones_39_46_roughness.jpg");
        auto grassAO = std::make_shared<Texture>("assets/textures/grass/grass_with_mud_and_stones_39_46_ao.jpg");
       // auto grassRoughness = std::make_shared<Texture>("assets/textures/grass/");
        grassMaterial->SetDiffuseTexture(grassDiffuse);
        grassMaterial->SetNormalTexture(grassNormal);
        grassMaterial->SetRoughnessTexture(grassRoughness);
        grassMaterial->SetAOTexture(grassAO);
        grassMaterial->SetTextureScale({ 30.0f, 30.0f });
        mesh->SetMaterial(grassMaterial);
        SetMesh(std::move(mesh));
	}
}
