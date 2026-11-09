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

        return m_Buffer != nullptr && m_LocalBuffer != nullptr;
    }

    void LightSystem::Upload(FrameContext& ctx)
    {
        if (!m_Buffer || !m_LocalBuffer) return;

        const DirectX::XMFLOAT3 cameraPosition = ctx.camera ? ctx.camera->GetPositionFloat3() : DirectX::XMFLOAT3{ 0, 0, 0 };
        uint32_t dropped = PackLights(ctx.scene.world, ctx.environment, cameraPosition, m_Data, m_Local);

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

        ctx.stats.directionalLights = m_Data.directionalLightCount;
        ctx.stats.localLights       = usable;
        ctx.stats.lightsDropped     = dropped;
    }
}
