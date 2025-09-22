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

        // Override update to sync light position
        void Update(float deltaTime = 0.0f) override;

    private:
        void Initialize();

    private:
    };

}