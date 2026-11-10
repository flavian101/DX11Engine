#include "dxpch.h"
#include "LightSystem.h"
#include "../Cache/ResourceCache.h"
#include "RHI/GraphicsDevice.h"
#include "camera/Camera.h"
#include "utils/ConstantBufferTypes.h"
#include "LightPacking.h"
#include <algorithm>

namespace DXEngine
{
    namespace
    {
        constexpr const char* kLocalLightBufferName = "Scene.LocalLights";
    }

    bool LightSystem::Initialize(RenderServices& services)
    {
        m_Buffer = services.resources->GetOrCreateConstantBuffer("Scene.Lights", sizeof(SceneLightData));

        m_LocalCapacity = kMinLocalLightCapacity;
        m_LocalBuffer   = services.resources->GetOrCreateStructuredBuffer(kLocalLightBufferName, sizeof(LightGPU), m_LocalCapacity);

        m_ClusterBuffer = services.resources->GetOrCreateConstantBuffer("Scene.Clusters", sizeof(ClusterConstants));
        m_ClusterRanges = services.resources->GetOrCreateStructuredBuffer("Scene.ClusterRanges", sizeof(ClusterRange), m_Clusterer.ClusterCount());
        m_LightIndices  = services.resources->GetOrCreateStructuredBuffer("Scene.ClusterLightIndices", sizeof(uint32_t), m_Clusterer.Desc().maxLightIndices);

        return m_Buffer && m_LocalBuffer && m_ClusterBuffer && m_ClusterRanges && m_LightIndices;
    }

    void LightSystem::Upload(FrameContext& ctx)
    {
        if (!m_Buffer || !m_LocalBuffer || !m_ClusterBuffer || !m_ClusterRanges || !m_LightIndices) return;

        const DirectX::XMFLOAT3 cameraPosition = ctx.camera ? ctx.camera->GetPositionFloat3() : DirectX::XMFLOAT3{ 0, 0, 0 };
        uint32_t dropped = PackLights(ctx.scene.world, ctx.environment, cameraPosition, m_Data, m_Local, kMaxLocalLights, &ctx.shadows.lights);

        // Grow the structured buffer when the scene outgrew it. Replacing it is safe: the FrameContext pointers
        // to the old one were cleared by BeginFrame and are set again below, and D3D11 keeps a bound resource alive
        // until it is unbound.
        const uint32_t wanted = LightBufferCapacityFor(static_cast<uint32_t>(m_Local.size()));
        if (wanted > m_LocalCapacity)
        {
            auto bigger = ctx.services->resources->GetOrCreateStructuredBuffer(kLocalLightBufferName, sizeof(LightGPU), wanted);
            if (bigger)
            {
                m_LocalBuffer   = std::move(bigger);
                m_LocalCapacity = wanted;
            }
            // else: keep the old buffer and draw what fits (counted as dropped below)
        }

        const uint32_t usable = (std::min)(static_cast<uint32_t>(m_Local.size()), m_LocalCapacity);
        dropped += static_cast<uint32_t>(m_Local.size()) - usable;
        m_Data.localLightCount = usable;

        m_Buffer->Update(&m_Data, sizeof(SceneLightData), 0);
        if (usable > 0)
            m_LocalBuffer->Update(m_Local.data(), usable * static_cast<uint32_t>(sizeof(LightGPU)), 0);

        ctx.lightBuffer      = m_Buffer.get();
        ctx.localLightBuffer = m_LocalBuffer.get();

        // ---- cluster the lights (skipped when the mode is None or the camera cannot be clustered)
        bool clustered = false;
        if (ctx.environment.lightCulling != LightCullingMode::None && ctx.camera && ctx.targets.backBuffer)
        {
            DirectX::XMFLOAT4X4 view, projection;
            DirectX::XMStoreFloat4x4(&view, ctx.camera->GetViewMatrix());
            DirectX::XMStoreFloat4x4(&projection, ctx.camera->GetProjectionMatrix());

            // The render target is what the pixel shader's SV_Position is measured in.
            if (m_Clusterer.SetCamera(view, projection, ctx.targets.backBuffer->GetWidth(), ctx.targets.backBuffer->GetHeight()))
            {
                m_Clusterer.Assign(m_Local.data(), usable);
                clustered = true;

                m_ClusterRanges->Update(m_Clusterer.Ranges().data(), m_Clusterer.ClusterCount() * static_cast<uint32_t>(sizeof(ClusterRange)), 0);
                if (m_Clusterer.IndexCount() > 0)
                    m_LightIndices->Update(m_Clusterer.Indices(), m_Clusterer.IndexCount() * static_cast<uint32_t>(sizeof(uint32_t)), 0);
            }
        }

        const ClusterConstants clusterConstants = m_Clusterer.Constants(clustered ? static_cast<uint32_t>(ctx.environment.lightCulling) : 0u);
        m_ClusterBuffer->Update(&clusterConstants, sizeof(ClusterConstants), 0);

        ctx.clusterBuffer      = m_ClusterBuffer.get();
        ctx.clusterRangeBuffer = m_ClusterRanges.get();
        ctx.lightIndexBuffer   = m_LightIndices.get();

        ctx.stats.directionalLights = m_Data.directionalLightCount;
        ctx.stats.localLights       = usable;
        ctx.stats.lightsDropped     = dropped;
        if (clustered)
        {
            ctx.stats.clusterAssignments = m_Clusterer.Stats().assignments;
            ctx.stats.clusterMaxLights   = m_Clusterer.Stats().maxLightsPerCluster;
            ctx.stats.clusterDropped     = m_Clusterer.Stats().assignmentsDropped;
        }
    }
}
