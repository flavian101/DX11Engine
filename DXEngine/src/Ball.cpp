#include "dxpch.h"
#include "Ball.h"
#include <utils/material/Material.h>

namespace DXEngine {

    Ball::Ball()
        : Model()
    {
        Initialize();

        auto moonMaterial = DXEngine::MaterialFactory::CreateLitMaterial("MoonMaterial");
        auto moonTexture = std::make_shared<Texture>("assets/textures/8k_moon.jpg");
        moonMaterial->SetDiffuseTexture(moonTexture);
        moonMaterial->SetSpecularColor({ 0.2f, 0.2f, 0.2f, 1.0f });
        moonMaterial->SetShininess(100.0f);

        GetMesh()->SetMaterial(moonMaterial);
    }

    void Ball::Initialize()
    {
        // Create sphere mesh using the factory method
        float radius = 1.0f;
        uint32_t segments = 64;

        auto sphereMesh = Mesh::CreateSphere(radius, segments);
        SetMesh(sphereMesh);
    }
}

