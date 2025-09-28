#include "dxpch.h"
#include "LightSphere.h"
#include "utils/Texture.h"
#include "utils/material/Material.h"



namespace DXEngine {


    LightSphere::LightSphere()
        : Model()
    {
        Initialize();

        auto emissiveMaterial = MaterialFactory::CreateEmissiveMaterial("LightSphere");

       emissiveMaterial->SetEmissiveColor({1.0f,0.5f,1.0f,1.0f});
       emissiveMaterial->SetDiffuseColor({1.0f,1.0f,1.0f,1.0f});

        GetMesh()->SetMaterial(emissiveMaterial);
    }

    void LightSphere::Initialize()
    {
        // Create small sphere for light representation
        float radius = 1.5f; 
        uint32_t segments = 16; // Lower detail for light sphere

        auto lightMesh = Mesh::CreateSphere(radius, segments);
        SetMesh(lightMesh);
    }

    void LightSphere::Update(float deltaTime)
    {
        Model::Update(deltaTime);
    }

}
