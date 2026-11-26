#include "dxpch.h"
#include "EntityBounds.h"
#include "utils/Mesh/Mesh.h"
#include "ViewportMath.h"

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

    EntityBounds ComputeSelectionBounds(const std::vector<Entity>& entities)
    {
        using namespace DirectX;
        std::vector<XMFLOAT3> points;
        for (const Entity& e : entities)
        {
            if (!e.IsValid() || !e.Has<TransformComponent>()) continue;
            const EntityBounds b = ComputeEntityBounds(e);
            if (b.hasBox) points.insert(points.end(), b.corners, b.corners + 8);
            else          points.push_back(b.center);
        }

        EntityBounds out;
        if (points.empty()) return out;
        XMFLOAT3 lo = points[0], hi = points[0];
        for (const XMFLOAT3& p : points)
        {
            lo = { std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z) };
            hi = { std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z) };
        }
        out.center = { (lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f };
        float r2 = 0.0f;
        for (const XMFLOAT3& p : points)
        {
            const float dx = p.x - out.center.x, dy = p.y - out.center.y, dz = p.z - out.center.z;
            r2 = std::max(r2, dx * dx + dy * dy + dz * dz);
        }
        out.radius = std::max(std::sqrt(r2), 0.05f);
        return out;
    }

    std::vector<Entity> EntitiesInRect(Scene& scene, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection,
                                       float width, float height, float x0, float y0, float x1, float y1)
    {
        std::vector<Entity> out;
        const float minX = std::min(x0, x1), maxX = std::max(x0, x1), minY = std::min(y0, y1), maxY = std::max(y0, y1);
        for (auto [id, transform] : scene.Registry().view<TransformComponent>().each())
        {
            Entity e = scene.Wrap(id);
            if (e.Has<SkyboxComponent>()) continue;
            const EntityBounds b = ComputeEntityBounds(e);
            DirectX::XMFLOAT2 s;
            if (!ViewportMath::ProjectToViewport(b.center, view, projection, width, height, s)) continue;    // behind the camera
            if (s.x >= minX && s.x <= maxX && s.y >= minY && s.y <= maxY) out.push_back(e);
        }
        return out;
    }
}
