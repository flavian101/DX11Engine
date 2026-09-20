#include "dxpch.h"
#include "Board.h"
#include "utils/material/Material.h"
#include <utils/mesh/Mesh.h>
#include "utils/Texture.h"
#include <utils/Mesh/Utils/VertexAttribute.h>
#include <utils/Mesh/Resource/MeshResource.h>
#include "Core/Input.h"


namespace DXEngine {

	Board::Board()

	{
        auto layout = VertexLayout::CreateLit();
        auto vertexData = std::make_unique<VertexData>(layout);
        vertexData->Resize(4); 

        float size = 1.0f; 
        DirectX::XMFLOAT3 positions[4] = {
            {-size, 0.0f, -size},
            { size, 0.0f, -size},
            { size, 0.0f,  size},
            {-size, 0.0f,  size} 
        };

        DirectX::XMFLOAT3 normal(0.0f, 1.0f, 0.0f); 
        DirectX::XMFLOAT4 tangent(1.0f, 0.0f, 0.0f, 1.0f); 

        DirectX::XMFLOAT2 texCoords[4] = {
            {0.0f, 1.0f},
            {1.0f, 1.0f},
            {1.0f, 0.0f},
            {0.0f, 0.0f} 
        };

        for (int i = 0; i < 4; ++i)
        {
            vertexData->SetAttribute(i, VertexAttributeType::Position, positions[i]);
            vertexData->SetAttribute(i, VertexAttributeType::Normal, normal);
            vertexData->SetAttribute(i, VertexAttributeType::TexCoord0, texCoords[i]);
            vertexData->SetAttribute(i, VertexAttributeType::Tangent, tangent);
        }

        auto indexData = std::make_unique<IndexData>(IndexType::UInt16);
        indexData->AddTriangle(0, 2, 1);
        indexData->AddTriangle(0, 3, 2);

        auto meshResource = std::make_shared<MeshResource>("Board Mesh");
        meshResource->SetVertexData(std::move(vertexData));
        meshResource->SetIndexData(std::move(indexData));
        meshResource->SetTopology(PrimitiveTopology::TriangleList);

        meshResource->GenerateBounds();

        auto mesh = std::make_shared<Mesh>(meshResource);

        auto metalMaterial = MaterialFactory::CreateLitMaterial("Board Material");
        auto boardDiffuse = std::make_shared<Texture>("assets/textures/metal/metalpanel.jpg");
        auto boardNormal = std::make_shared<Texture>("assets/textures/metal/Normal_Map.png");
        auto boardMetal = std::make_shared<Texture>("assets/textures/metal/Metalness_Map.png");
        metalMaterial->SetDiffuseTexture(boardDiffuse);
        metalMaterial->SetNormalTexture(boardNormal);
        metalMaterial->SetMetallicTexture(boardMetal);
        metalMaterial->SetTextureScale({ 10.0f,10.0f });
        metalMaterial->SetNormalScale(1.50f);
        mesh->SetMaterial(metalMaterial);
        SetMesh(std::move(mesh));
    }
    Board::~Board()
    {
    }

    BoardController::BoardController()
        :
        m_Pitch(0.0f),
        m_Roll(0.0f),
        m_TiltSpeed(10.0f),
        m_MaxTilt(89.0f),
        m_Orientation({0.0f,0.0f,0.0f})
    {
    }

    BoardController::~BoardController()
    {
    }

    DirectX::XMFLOAT3 BoardController::Update(float deltaTime)
    {
        float change = m_TiltSpeed * deltaTime;

        if (DXEngine::Input::IsKeyPressed(VK_UP))
        {
            m_Pitch += change;
        }
        if (DXEngine::Input::IsKeyPressed(VK_DOWN))
        {
            m_Pitch -= change;
        }
        if (DXEngine::Input::IsKeyPressed(VK_RIGHT))
        {
            m_Roll += change;
        }
        if (DXEngine::Input::IsKeyPressed(VK_LEFT)) 
        {
            m_Roll -= change;
        }

        m_Pitch = std::clamp(m_Pitch, -m_MaxTilt, m_MaxTilt);
        m_Roll = std::clamp(m_Roll, -m_MaxTilt, m_MaxTilt);

        m_Orientation.x = m_Pitch;
        m_Orientation.z = m_Roll;

        return m_Orientation;

    }

}