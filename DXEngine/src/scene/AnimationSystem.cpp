#include "dxpch.h"
#include "AnimationSystem.h"
#include "Scene.h"

namespace DXEngine
{
    namespace AnimationSystem
    {
        void Update(Scene& scene, float deltaSeconds)
        {
            for (auto [entity, hook] : scene.Registry().view<AnimationUpdateComponent>().each())
                if (hook.update)
                    hook.update(deltaSeconds);
        }
    }
}
