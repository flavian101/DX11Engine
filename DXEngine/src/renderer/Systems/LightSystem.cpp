#include "dxpch.h"
#include "LightSystem.h"
#include "../Cache/ResourceCache.h"
#include "RHI/GraphicsDevice.h"
#include "camera/Camera.h"
#include "utils/ConstantBufferTypes.h"
#include "LightPacking.h"

namespace DXEngine
{
    bool LightSystem::Initialize(RenderServices& services)
    {
        m_Buffer = services.resources->GetOrCreateConstantBuffer("Scene.Lights", sizeof(SceneLightData));
        return m_Buffer != nullptr;
    }

    void LightSystem::Upload(FrameContext& ctx)
    {
        if (!m_Buffer) return;

        const DirectX::XMFLOAT3 cameraPosition = ctx.camera ? ctx.camera->GetPositionFloat3() : DirectX::XMFLOAT3{ 0, 0, 0 };
        PackLights(ctx.scene.world, ctx.environment, cameraPosition, m_Data);

        m_Buffer->Update(&m_Data, sizeof(SceneLightData), 0);
        ctx.lightBuffer = m_Buffer.get();
    }
}
