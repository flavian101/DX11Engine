#pragma once

namespace DXEngine
{
    class Scene;

    // Advances everything that animates. Run once per frame with the frame's delta in SECONDS, before
    // TransformSystem::Update.
    namespace AnimationSystem
    {
        void Update(Scene& scene, float deltaSeconds);
    }
}
