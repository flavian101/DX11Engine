#include "dxpch.h"
#include "LightSystem.h"
#include "../Cache/ResourceCache.h"
#include "RHI/GraphicsDevice.h"
#include "camera/Camera.h"
#include "utils/ConstantBufferTypes.h"
#include "utils/Light.h"

namespace DXEngine
{
    bool LightSystem::Initialize(RenderServices& services)
    {
        m_Buffer = services.resources->GetOrCreateConstantBuffer("Scene.Lights", sizeof(SceneLightData));
        return m_Buffer != nullptr;
    }

    void LightSystem::Upload(FrameContext& ctx)
    {
        const auto& lights = ctx.services->lights;
        if (!lights || !m_Buffer) return;                  // no lights: lightBuffer stays null

        // needs the frustum (and camera position for sorting), so run AFTER CullingSystem
        lights->CullLights(ctx.visibility.viewFrustum,
                           ctx.camera ? ctx.camera->GetPositionFloat3() : DirectX::XMFLOAT3{ 0, 0, 0 });
        lights->UpdateLightData();                         // fills LightManager's SceneLightData (CPU only)

        const SceneLightData& data = lights->GetSceneData();
        m_Buffer->Update(&data, sizeof(SceneLightData), 0);
        ctx.lightBuffer = m_Buffer.get();
    }
}
