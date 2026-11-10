#include "dxpch.h"
#include "ShadowSystem.h"
#include "../Cache/ResourceCache.h"
#include "RHI/GraphicsDevice.h"
#include "utils/ConstantBufferTypes.h"
#include <algorithm>

namespace DXEngine
{
    namespace
    {
        constexpr const char* kViewBufferName = "Shadow.Views";
        constexpr const char* kAtlasName      = "Shadow.Atlas";
        constexpr uint32_t    kMinViewCapacity = 64;      // 6 KB: enough for a sun + several shadowed lights

        uint32_t CapacityFor(uint32_t views)
        {
            uint32_t c = kMinViewCapacity;
            while (c < views) c <<= 1;
            return c;
        }
    }

    bool ShadowSystem::Initialize(RenderServices& services)
    {
        m_Constants    = services.resources->GetOrCreateConstantBuffer("Shadow.Constants", sizeof(ShadowConstants));
        m_ViewCapacity = kMinViewCapacity;
        m_ViewBuffer   = services.resources->GetOrCreateStructuredBuffer(kViewBufferName, sizeof(ShadowViewGPU), m_ViewCapacity);
        return m_Constants && m_ViewBuffer;
    }

    void ShadowSystem::Update(FrameContext& ctx, const SceneExtraction& extraction, const ExtractionCamera& camera)
    {
        if (!m_Constants || !m_ViewBuffer) return;
        const ShadowSettings& settings = ctx.environment.shadows;

        m_Planner.Plan(ctx.scene.world, camera, settings, extraction, ctx.shadows, ctx.stats);

        // ---- the atlas + the view buffer: only when something was planned
        if (!ctx.shadows.views.empty())
        {
            const uint32_t viewCount = static_cast<uint32_t>(ctx.shadows.gpuViews.size());
            const uint32_t wanted = CapacityFor(viewCount);
            if (wanted > m_ViewCapacity)
            {
                // Replacing is safe: BeginFrame cleared the FrameContext pointers and D3D11 keeps a bound resource alive
                // until it is unbound.
                if (auto bigger = ctx.services->resources->GetOrCreateStructuredBuffer(kViewBufferName, sizeof(ShadowViewGPU), wanted))
                {
                    m_ViewBuffer   = std::move(bigger);
                    m_ViewCapacity = wanted;
                }
            }

            std::shared_ptr<RHI::ITexture> atlas =
                ctx.services->resources->GetOrCreateDepthTexture(kAtlasName, ctx.shadows.atlasSize, ctx.shadows.atlasSize, RHI::TextureFormat::D32_FLOAT);

            if (!atlas || viewCount > m_ViewCapacity)
            {
                // No atlas (the device refused) or no room for the views: render NO shadows this frame. Clearing the
                // light table too, so no light points at a view that does not exist.
                ctx.stats.shadowLightsDropped += static_cast<uint32_t>(ctx.shadows.lights.size());
                ctx.stats.shadowedDirectional = ctx.stats.shadowedLocal = ctx.stats.shadowViews = 0;
                ctx.shadows.Clear();
            }
            else
            {
                m_ViewBuffer->Update(ctx.shadows.gpuViews.data(), viewCount * static_cast<uint32_t>(sizeof(ShadowViewGPU)), 0);
                ctx.shadows.atlas      = atlas.get();      // the ResourceCache owns it
                ctx.shadows.viewBuffer = m_ViewBuffer.get();
            }
        }

        // ---- the constants are valid every frame (a lit shader may read b5 even when no light has a shadow)
        ShadowConstants constants{};
        const float atlasSize = static_cast<float>((std::max)(ctx.shadows.atlasSize, 1u));
        constants.atlasSize    = atlasSize;
        constants.invAtlasSize = 1.0f / atlasSize;
        constants.pcfRadius    = (std::min)(settings.pcfRadius, 3u);
        constants.enabled      = ctx.shadows.HasWork() ? 1u : 0u;
        m_Constants->Update(&constants, sizeof(constants), 0);
        ctx.shadows.constantBuffer = m_Constants.get();
    }
}
