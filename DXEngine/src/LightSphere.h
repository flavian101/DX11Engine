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
        // The sphere is the light's visible marker AND its owner: it creates a point light and carries it to
        // wherever the sphere is (see Update). Before, nothing connected the sphere to the scene's only point
        // light, which sat at a different fixed position.
        std::shared_ptr<PointLight> m_Light;
    };

}