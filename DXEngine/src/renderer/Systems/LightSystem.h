#pragma once
#include "../Core/FrameContext.h"
#include "utils/ConstantBufferTypes.h"
#include <memory>

namespace DXEngine
{
    namespace RHI { class IBuffer; }

    // CPU system. Packs the Scene's lights + the environment (PackLights) and uploads them.
    // Reads: ctx.scene.world, ctx.environment, ctx.camera.   Writes: ctx.lightBuffer (ALWAYS valid: the
    // environment values - exposure, gamma, ambient - are in this block even when there are no lights).
    class LightSystem
    {
    public:
        bool Initialize(RenderServices& services);
        void Upload(FrameContext& ctx);

    private:
        std::shared_ptr<RHI::IBuffer> m_Buffer;
        SceneLightData m_Data;   // reused every frame: 9 KB, not worth a stack copy
    };
}
