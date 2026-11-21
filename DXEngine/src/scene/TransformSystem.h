#pragma once
#include <cstddef>

namespace DXEngine
{
    class Scene;

    // Computes TransformComponent::world (parent's world, then this entity's local transform on top).
    // Run once per frame by the application, after gameplay code and AnimationSystem have changed positions / rotations
    // and before anything that reads world matrices (culling, rendering, lights). The renderer does NOT call it: it only
    // reads the scene.
    //
    // Only what changed is recomputed. An entity needs it when its position / rotation / scale differ from what its
    // world matrix was computed from, or when it was marked dirty (a new entity, a new parent, a copied transform); then
    // it and everything below it get a new world matrix, each exactly once. Entities whose chain of ancestors did not
    // change are not touched. The cost is one comparison per entity plus the size of the changed subtrees.
    //
    // The one change the fields cannot show: removing the TransformComponent of an entity that has children (their world
    // stops including it). Call MarkDirty() on the children if that is ever done; nothing does it today.
    namespace TransformSystem
    {
        // Returns how many world matrices were recomputed (0 = every transform was current).
        std::size_t Update(Scene& scene);

        // true if every world matrix is current, i.e. Update would do nothing. One comparison per entity: for tests and
        // debug checks, not for the frame loop (Update itself is the cheap way to find out).
        bool IsCurrent(Scene& scene);
    }
}
