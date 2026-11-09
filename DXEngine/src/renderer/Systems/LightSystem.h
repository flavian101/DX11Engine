#pragma once
#include "../Core/FrameContext.h"
#include "utils/ConstantBufferTypes.h"
#include <memory>
#include <vector>

namespace DXEngine
{
    namespace RHI { class IBuffer; }

    // CPU system. Packs the Scene's lights + the environment (PackLights) and uploads them:
    //   - SceneLightData (environment + directional lights + local light count)  -> constant buffer
    //   - the point + spot lights                                                 -> structured buffer
    // The structured buffer grows in doubling steps (up to kMaxLocalLights), so a scene is not capped by a
    // constant buffer any more. It is owned by the ResourceCache; this system only asks for a bigger one.
    //
    // Reads: ctx.scene.world, ctx.environment, ctx.camera.
    // Writes: ctx.lightBuffer and ctx.localLightBuffer (ALWAYS valid: the environment values - exposure, gamma,
    //         ambient - are in the constant block even when there are no lights), ctx.stats (light counts).
    class LightSystem
    {
    public:
        bool Initialize(RenderServices& services);
        void Upload(FrameContext& ctx);

    private:
        std::shared_ptr<RHI::IBuffer> m_Buffer;        // constant buffer b3
        std::shared_ptr<RHI::IBuffer> m_LocalBuffer;   // StructuredBuffer<LightGPU>
        uint32_t m_LocalCapacity = 0;                  // elements m_LocalBuffer can hold

        SceneLightData        m_Data;    // reused every frame (a constant block of a few KB is not worth a stack copy)
        std::vector<LightGPU> m_Local;   // reused every frame: capacity is kept
    };
}
