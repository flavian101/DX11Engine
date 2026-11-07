#pragma once

namespace DXEngine
{
    class Scene;

    // Computes TransformComponent::world for every entity, parents before children.
    // Run once per frame, after gameplay code has changed positions/rotations and before anything
    // that reads world matrices (culling, rendering, lights).
    namespace TransformSystem
    {
        void Update(Scene& scene);
    }
}
