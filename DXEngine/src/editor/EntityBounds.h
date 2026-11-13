#pragma once
#include "scene/Scene.h"

namespace DXEngine
{
    // The world-space box an entity occupies (what the viewport outlines and what "frame selection" aims at).
    struct EntityBounds
    {
        bool hasBox = false;                      // false: no mesh, 'center' is the entity's position and 'radius' a small marker size
        DirectX::XMFLOAT3 corners[8];             // the mesh's local box moved to world space (only when hasBox)
        DirectX::XMFLOAT3 center{ 0, 0, 0 };
        float radius = 0.5f;                      // from the center to the farthest corner
    };

    // Needs current world matrices (TransformSystem::Update). An invalid entity gives hasBox = false at the origin.
    EntityBounds ComputeEntityBounds(Entity entity);
}
