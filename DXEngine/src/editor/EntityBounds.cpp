#include "dxpch.h"
#include "EntityBounds.h"
#include "utils/Mesh/Mesh.h"

namespace DXEngine
{
    EntityBounds ComputeEntityBounds(Entity entity)
    {
        using namespace DirectX;
        EntityBounds out;
        if (!entity.IsValid() || !entity.Has<TransformComponent>())
            return out;

        const TransformComponent& t = entity.Get<TransformComponent>();
        out.center = t.WorldPosition();

        const MeshRendererComponent* mr = entity.TryGet<MeshRendererComponent>();
        if (!mr || !mr->mesh)
            return out;                           // no mesh: a marker at the position

        const BoundingBox& box = mr->mesh->GetBoundingBox();
        const XMMATRIX world = t.World();
        XMVECTOR sum = XMVectorZero();
        for (int i = 0; i < 8; ++i)
        {
            const XMVECTOR local = XMVectorSet((i & 1) ? box.max.x : box.min.x,
                                               (i & 2) ? box.max.y : box.min.y,
                                               (i & 4) ? box.max.z : box.min.z, 1.0f);
            const XMVECTOR w = XMVector3TransformCoord(local, world);
            XMStoreFloat3(&out.corners[i], w);
            sum += w;
        }
        XMStoreFloat3(&out.center, sum / 8.0f);

        float r2 = 0.0f;
        const XMVECTOR c = XMLoadFloat3(&out.center);
        for (const XMFLOAT3& p : out.corners)
            r2 = std::max(r2, XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&p) - c)));
        out.radius = std::max(std::sqrt(r2), 0.05f);
        out.hasBox = true;
        return out;
    }
}
