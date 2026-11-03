#include "dxpch.h"
#include "CullingSystem.h"
#include "camera/Camera.h"
#include "models/Model.h"

namespace DXEngine
{
    void CullingSystem::Cull(FrameContext& ctx)
    {
        ctx.visibility.Clear();
        ctx.stats.objectsSubmitted = static_cast<uint32_t>(ctx.scene.models.size());
        ctx.stats.objectsCulled = 0;

        if (ctx.scene.models.empty() || !ctx.camera)
            return;

        ctx.visibility.viewFrustum = BuildFrustum(ctx);   // once per frame

        for (const SceneModel& entry : ctx.scene.models)
        {
            const Model* model = entry.model.get();
            if (!model || !model->IsValid() || !model->IsVisible())
                continue;   // invisible on purpose: not counted as "culled"

            if (TestModel(model, ctx) == CullResult::Visible)
                ctx.visibility.visibleModels.push_back(&entry);
            else
                ++ctx.stats.objectsCulled;
        }
        // NOTE: shadow casters are intentionally NOT produced here. A caster outside the
        // camera frustum can still shadow something inside it, so the future ShadowPass
        // needs its own test against the LIGHT's frustum. Add it with that pass.
    }

    CullResult CullingSystem::TestModel(const Model* model, const FrameContext& ctx) const
    {
        const BoundingSphere s = model->GetWorldBoundingSphere();   // already includes transform + scale

        if (m_Config.enableDistanceCulling && ctx.camera)
        {
            const DirectX::XMFLOAT3 c = ctx.camera->GetPositionFloat3();
            const float dx = s.center.x - c.x, dy = s.center.y - c.y, dz = s.center.z - c.z;
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist - s.radius > m_Config.maxDrawDistance)       // subtract radius: big objects stay
                return CullResult::DistanceCulled;
        }

        if (m_Config.enableFrustumCulling)
        {
            DirectX::BoundingSphere sphere;
            sphere.Center = s.center;
            sphere.Radius = s.radius * m_Config.frustumMarginScale;
            if (ctx.visibility.viewFrustum.Contains(sphere) == DirectX::DISJOINT)
                return CullResult::FrustumCulled;
        }
        return CullResult::Visible;
    }

    DirectX::BoundingFrustum CullingSystem::BuildFrustum(const FrameContext& ctx) const
    {
        DirectX::BoundingFrustum frustum;
        DirectX::BoundingFrustum::CreateFromMatrix(frustum, ctx.camera->GetProjectionMatrix());
        // CreateFromMatrix gives a VIEW-space frustum; move it to world space.
        const DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, ctx.camera->GetViewMatrix());
        frustum.Transform(frustum, invView);
        return frustum;
    }
}
