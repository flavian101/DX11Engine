#pragma once
#include "../Core/RenderTypes.h"
#include "utils/ConstantBufferTypes.h"   // SceneLightData, LightGPU
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace DXEngine
{
    class Scene;

    // Hard limit of local (point + spot) lights per frame: 8192 x 64 bytes = 512 KB of GPU memory.
    // Above it the lights nearest to the camera win. (The old constant-buffer design stopped at 64 + 32.)
    constexpr uint32_t kMaxLocalLights = 8192;

    // The structured buffer starts small and doubles when a scene needs more (see LightBufferCapacityFor).
    constexpr uint32_t kMinLocalLightCapacity = 256;

    // Fills the GPU light data from the Scene's LightComponents plus the environment settings:
    //   out          the constant buffer block: environment + DIRECTIONAL lights (max 4) + localLightCount
    //   localLights  the point + spot lights, one LightGPU each, for the structured buffer. Cleared first; its
    //                capacity is kept, so a steady scene allocates nothing per frame.
    // Lights that cannot contribute are skipped: switched off (intensity <= 0) and local lights with no range.
    // Pure CPU logic (no GPU calls), so it is unit-tested.
    //
    // Returns how many local lights were DROPPED because the scene has more than 'maxLocalLights' (the ones
    // nearest to 'cameraPosition' are kept, in no particular order). 0 in every normal case.
    //
    // 'scene' may be null: only the environment is written.
    uint32_t PackLights(Scene* scene, const RenderEnvironment& environment,
                        const DirectX::XMFLOAT3& cameraPosition,
                        SceneLightData& out, std::vector<LightGPU>& localLights,
                        uint32_t maxLocalLights = kMaxLocalLights);

    // How many LightGPU elements the GPU buffer should have for 'lightCount' lights: the next power of two,
    // never below kMinLocalLightCapacity, never above kMaxLocalLights. Growing in doubling steps keeps buffer
    // re-creation rare while a scene fills up.
    uint32_t LightBufferCapacityFor(uint32_t lightCount);
}
