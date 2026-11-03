#pragma once
#include "../Core/FrameContext.h"
#include <memory>

namespace DXEngine
{
    namespace RHI { class IBuffer; }

    // CPU system (replaces the empty LightCuller stub).
    // Reads: ctx.services->lights, ctx.visibility.viewFrustum.   Writes: ctx.lightBuffer.
    class LightSystem
    {
    public:
        bool Initialize(RenderServices& services);
        void Upload(FrameContext& ctx);

    private:
        std::shared_ptr<RHI::IBuffer> m_Buffer;
    };
}
