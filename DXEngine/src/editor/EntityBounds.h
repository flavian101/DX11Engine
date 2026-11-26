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

    // One box around all of them (hasBox = false: it is a sphere, for framing): the center of the entities' bounds' union and the
    // radius that reaches all of their corners. Invalid entities are ignored; none at all gives the origin.
    EntityBounds ComputeSelectionBounds(const std::vector<Entity>& entities);

    // The entities whose bounds' CENTER falls inside the rectangle (x0, y0)-(x1, y1), in the pixels of a picture 'width' x 'height'
    // seen through 'view' / 'projection': what a box select picks. (The center, not "any part of it": a ground plane under everything
    // is not picked by every box.) The sky is never picked. Needs current world matrices.
    std::vector<Entity> EntitiesInRect(Scene& scene, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection,
                                       float width, float height, float x0, float y0, float x1, float y1);
}
