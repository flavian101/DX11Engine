#include "dxpch.h"
#include "LightSphere.h"
#include "renderer/Renderer.h"
#include "utils/Light.h"
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

        // Create the light this sphere stands for (needs Renderer::InitLightManager() to have run).
        if (auto lights = Renderer::GetLightManager())
        {
            m_Light = lights->CreatePointLight();
            if (m_Light)
            {
                m_Light->SetColor({ 1.0f, 1.0f, 1.0f });
                m_Light->SetIntensity(30000.0f);
                m_Light->SetRadius(20.0f);
                m_Light->SetAttenuation({ 700.0f, 400.0f, 100.0f });
                m_Light->SetCastShadows(false);
            }
        }
        else
        {
            OutputDebugStringA("WARNING: LightSphere created before Renderer::InitLightManager - it will not light anything\n");
        }
    }

    void LightSphere::Initialize()
    {
        // Create small sphere for light representation
        float radius = 1.5f; 
        uint32_t segments = 16; // Lower detail for light sphere

        auto lightMesh = Mesh::CreateSphere(Renderer::GetDevice(), radius, segments);
        SetMesh(lightMesh);
    }

    void LightSphere::Update(float deltatime)
    {
        Model::Update(deltatime);

        // keep the light where the sphere is
        if (m_Light)
        {
            DirectX::XMFLOAT3 position;
            DirectX::XMStoreFloat3(&position, GetTranslation());
            m_Light->SetPosition(position);
        }
    }

}
