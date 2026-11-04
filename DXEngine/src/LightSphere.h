#pragma once
#include "utils/Light.h"
#include "utils/Mesh/Mesh.h"
#include "models/Model.h"
#include <memory>

namespace DXEngine {

    class LightSphere : public Model
    {
    public:
        LightSphere();

        // Override update to sync light position
        void Update(FrameTime deltatime) override;

    private:
        void Initialize();

    private:
    };

}