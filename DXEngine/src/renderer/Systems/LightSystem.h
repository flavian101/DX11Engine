#pragma once
#include "../Core/FrameContext.h"
#include "LightClustering.h"
#include "utils/ConstantBufferTypes.h"
#include <memory>
#include <vector>

namespace DXEngine
{
    namespace RHI { class IBuffer; }

    // CPU system. Packs the Scene's lights + the environment (PackLights), sorts the local lights into the
    // cluster grid (LightClusterer) and uploads everything the pixel shader reads:
    //   - SceneLightData (environment + directional lights + local light count)  -> constant buffer  b3
    //   - the point + spot lights                                                 -> structured buffer t32
    //   - the cluster grid constants                                              -> constant buffer  b7
    //   - per-cluster {offset, count} and the cluster light index list            -> structured buffers t33, t34
    // The light buffer grows in doubling steps (up to kMaxLocalLights). All buffers are owned by the ResourceCache;
    // this system only asks for them.
    //
    // Reads: ctx.scene.world, ctx.environment (lightCulling), ctx.camera, ctx.targets.color (screen size).
    // Writes: ctx.lightBuffer, ctx.localLightBuffer, ctx.clusterBuffer, ctx.clusterRangeBuffer, ctx.lightIndexBuffer
    //         (ALWAYS valid: the environment values - exposure, gamma, ambient - are in the constant block even when
    //         there are no lights), ctx.stats (light and cluster counts).
    //
    // With LightCullingMode::None (or a camera clustering cannot serve) the cluster work is skipped and the
    // constants tell the shader to loop over all lights.
    class LightSystem
    {
    public:
        bool Initialize(RenderServices& services);
        void Upload(FrameContext& ctx);

    private:
        std::shared_ptr<RHI::IBuffer> m_Buffer;          // constant buffer b3
        std::shared_ptr<RHI::IBuffer> m_LocalBuffer;     // StructuredBuffer<LightGPU>
        uint32_t m_LocalCapacity = 0;                    // elements m_LocalBuffer can hold

        LightClusterer                m_Clusterer;
        std::shared_ptr<RHI::IBuffer> m_ClusterBuffer;   // constant buffer b7
        std::shared_ptr<RHI::IBuffer> m_ClusterRanges;   // StructuredBuffer<uint2>, one element per cluster
        std::shared_ptr<RHI::IBuffer> m_LightIndices;    // StructuredBuffer<uint>, the cluster light index list

        SceneLightData        m_Data;    // reused every frame (a constant block of a few KB is not worth a stack copy)
        std::vector<LightGPU> m_Local;   // reused every frame: capacity is kept
    };
}
