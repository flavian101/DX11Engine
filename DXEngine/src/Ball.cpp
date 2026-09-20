#include "dxpch.h"
#include "Ball.h"
#include <utils/material/Material.h>

namespace DXEngine {

    Ball::Ball()
        : Model()
    {
        Initialize();

        auto earthMaterial = DXEngine::MaterialFactory::CreateLitMaterial("EarthMaterial");
        auto earthTexture = std::make_shared<Texture>("assets/textures/earth/2k_earth_daymap_diffuse.png");
        auto earthNormal = std::make_shared<Texture>("assets/textures/earth/2k_earth_daymap_normal.png");
        auto earthRoughness = std::make_shared<Texture>("assets/textures/earth/2k_earth_daymap_roughness.png");
        
        earthMaterial->SetDiffuseTexture(earthTexture);
        earthMaterial->SetNormalTexture(earthNormal);
        earthMaterial->SetNormalScale(5.0f);
        earthMaterial->SetRoughnessTexture(earthRoughness);

        earthMaterial->SetShininess(100.0f);

        GetMesh()->SetMaterial(earthMaterial);
    }

    void Ball::Initialize()
    {
        // Create sphere mesh using the factory method
        float radius = 1.0f;
        uint32_t segments = 52;

        auto sphereMesh = Mesh::CreateSphere(radius, segments);
        SetMesh(sphereMesh);
    }
}

