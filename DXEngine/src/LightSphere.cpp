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

        m_Light = std::make_shared<PointLight>();
        emissiveMaterial->SetEmissiveColor(m_Light->GetLightColor());
        emissiveMaterial->SetDiffuseColor(m_Light->GetLightColor());

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

    void LightSphere::BindLight()
    {
        if (m_Light)
        {
            // Update light position based on model position
            DirectX::XMFLOAT3 lightPos;
            DirectX::XMStoreFloat3(&lightPos, GetTranslation());
            m_Light->SetPosition(lightPos);
            m_Light->Bind();
        }
    }

    void LightSphere::Update(float deltaTime)
    {
        Model::Update(deltaTime);

        // Update light position automatically
        if (m_Light)
        {
            DirectX::XMFLOAT3 lightPos;
            DirectX::XMStoreFloat3(&lightPos, GetTranslation());
            m_Light->SetPosition(lightPos);
        }
    }

    std::shared_ptr<PointLight> LightSphere::GetLight() const
    {
        return m_Light;
    }

    void LightSphere::SetLightColor(const DirectX::XMFLOAT4& color)
    {
        if (m_Light)
        {
           // m_Light->SetLightColor(color);

            // Update material to match light color
            auto material = GetMaterial();
            if (material)
            {
                material->SetEmissiveColor(color);
                material->SetDiffuseColor(color);
            }
        }
    }

    void LightSphere::SetLightIntensity(float intensity)
    {
        if (m_Light)
        {
            //m_Light->SetIntensity(intensity);
        }
    }
}
