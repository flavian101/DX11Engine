#include "dxpch.h"
#include "SkySphere.h"
#include <utils/material/Material.h>
#include "utils/CubeMapTexture.h"

namespace DXEngine {

    SkySphere::SkySphere()
        : Model()
    {
        Initialize();

        auto skyMaterial = DXEngine::MaterialFactory::CreateSkyboxMaterial("SkyMaterial");

        const char* skyFilenames[6] = {
            "assets/textures/NightSky/nightBack.png",
            "assets/textures/NightSky/nightBottom.png",
            "assets/textures/NightSky/nightFront.png",
            "assets/textures/NightSky/nightLeft.png",
            "assets/textures/NightSky/nightRight.png",
            "assets/textures/NightSky/nightTop.png"
        };

        auto skyTexture = std::make_shared<CubeMapTexture>(skyFilenames);
        skyMaterial->SetEnvironmentTexture(skyTexture);
        skyMaterial->SetEmissiveColor({ 0.01f, 0.01f, 0.05f, 0.4f });

         // Create large sphere for skybox
        float radius = 50.0f; // Large radius for sky dome
        uint32_t segments = 32; // Medium detail for sky sphere

        auto skyMesh = Mesh::CreateSphere(radius, segments);


        // Sky sphere should not cast shadows
        setCastsShadows(false);
        SetReceivesShadows(false);

        skyMesh->SetMaterial(skyMaterial);
        SetMesh(skyMesh);

    }

    void SkySphere::Initialize()
    {
       
    }
}