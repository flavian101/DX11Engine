#pragma once
#include "../Core/FrameContext.h"
#include <DirectXCollision.h>
#include <string>

namespace DXEngine
{
    class Model;

    enum class CullResult : uint8_t { Visible, FrustumCulled, DistanceCulled };

    struct CullingConfig
    {
        bool  enableFrustumCulling = true;
        bool  enableDistanceCulling = false;
        float maxDrawDistance = 1000.0f;   // world units
        float frustumMarginScale = 1.05f;  // safety margin on bounding spheres
    };

    // CPU system.  Reads: ctx.scene, ctx.camera.   Writes: ctx.visibility, ctx.stats.objectsCulled.
    class CullingSystem
    {
    public:
        CullingSystem() = default;
        explicit CullingSystem(const CullingConfig& cfg) : m_Config(cfg) {}

        void Cull(FrameContext& ctx);
        CullResult TestModel(const Model* model, const FrameContext& ctx) const;

        void SetConfig(const CullingConfig& cfg) { m_Config = cfg; }
        const CullingConfig& GetConfig() const { return m_Config; }

    private:
        DirectX::BoundingFrustum BuildFrustum(const FrameContext& ctx) const;

        CullingConfig m_Config;
    };
}
