#pragma once
#include "Scene.h"
#include <cfloat>

namespace DXEngine
{
    class Camera;
    struct Ray;

    struct PickResult
    {
        Entity entity;                 // invalid when nothing was hit
        float distance = FLT_MAX;      // along the ray
        explicit operator bool() const { return entity.IsValid(); }
    };

    // Which entity is under the mouse? Tests the ray against every visible MeshRenderer's world-space box.
    // (Replaces PickingManager/InterfacePickable, which were tied to Model subclasses.) Sky entities are ignored.
    namespace ScenePicking
    {
        PickResult Raycast(Scene& scene, const Ray& ray);
        PickResult Pick(Scene& scene, const Camera& camera, float screenX, float screenY, int screenWidth, int screenHeight);
    }
}
