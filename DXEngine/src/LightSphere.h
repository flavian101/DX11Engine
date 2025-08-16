#pragma once
#include "utils/Light.h"
#include "utils/mesh/Mesh.h"
#include "models/Model.h"
#include <memory>

namespace DXEngine {

    class LightSphere : public Model
    {
    public:
        LightSphere();

        // Light management
        void BindLight();
        std::shared_ptr<PointLight> GetLight() const;

        // Light properties
        void SetLightColor(const DirectX::XMFLOAT4& color);
        void SetLightIntensity(float intensity);

        // Override update to sync light position
        void Update(float deltaTime = 0.0f) override;

    private:
        void Initialize();

    private:
        std::shared_ptr<PointLight> m_Light;
    };

}