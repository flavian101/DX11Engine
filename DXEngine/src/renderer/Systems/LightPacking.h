#pragma once
#include "../Core/RenderTypes.h"
#include "utils/ConstantBufferTypes.h"   // SceneLightData
#include <DirectXMath.h>

namespace DXEngine
{
    class Scene;

    // Fills the GPU light block from the Scene's LightComponents plus the environment settings.
    // Pure CPU logic (no GPU calls), so it is unit-tested.
    //
    // Transitional: the GPU block still has fixed capacities (4 directional, 64 point, 32 spot). When a scene has
    // more, the point/spot lights nearest to the camera win. The clustered-lighting milestone replaces this
    // block with a structured buffer of arbitrary size; callers will not change.
    //
    // 'scene' may be null: only the environment is written.
    void PackLights(Scene* scene, const RenderEnvironment& environment,
                    const DirectX::XMFLOAT3& cameraPosition, SceneLightData& out);
}
