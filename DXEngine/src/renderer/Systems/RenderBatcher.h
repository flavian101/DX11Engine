#pragma once
#include "../Core/FrameContext.h"
#include "../Core/RenderTypes.h"
#include <DirectXMath.h>

namespace DXEngine
{
    class Model;
    class Material;

    // CPU system.  Reads: ctx.visibility, ctx.camera.   Writes: ctx.batches.
    // Stateless between frames (the old SetGlobalMaterialOverride hidden state is gone:
    // overrides now travel with each SceneModel).
    class RenderBatcher
    {
    public:
        void BuildBatches(FrameContext& ctx);

    private:
        void AppendModel(const SceneModel& entry, const DirectX::XMFLOAT3& camPos, BatchQueue& out) const;
        static bool IsTransparent(const Material& material);
    };
}
