#include "dxpch.h"
#include "SceneExtraction.h"
#include "scene/Scene.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace DXEngine
{
    using namespace DirectX;

    namespace
    {
        // Sort policies (same as the old RenderBatcher): ONE place to change the batching strategy.
        bool OpaqueLess(const DrawItem& a, const DrawItem& b)
        {
            if (a.material != b.material)
                return std::less<const Material*>{}(a.material.get(), b.material.get());
            if (a.mesh != b.mesh)
                return std::less<const Mesh*>{}(a.mesh.get(), b.mesh.get());
            return a.distanceToCamera < b.distanceToCamera;
        }

        bool TransparentLess(const DrawItem& a, const DrawItem& b)
        {
            return a.distanceToCamera > b.distanceToCamera;
        }

        bool IsTransparent(const Material& m)
        {
            return m.GetType() == MaterialType::Transparent || m.GetBlendMode() != RHI::BlendMode::Opaque;
        }

        float AxisLength(float x, float y, float z) { return std::sqrt(x * x + y * y + z * z); }
    }

    BoundingFrustum SceneExtraction::BuildFrustum(const ExtractionCamera& camera)
    {
        BoundingFrustum frustum;
        BoundingFrustum::CreateFromMatrix(frustum, XMLoadFloat4x4(&camera.projection));
        // CreateFromMatrix gives a VIEW-space frustum; move it to world space.
        frustum.Transform(frustum, XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera.view)));
        return frustum;
    }

    void SceneExtraction::Extract(Scene& scene, const ExtractionCamera& camera, BatchQueue& out, RenderStatistics& stats) const
    {
        out.Clear();                               // keeps capacity: no per-frame allocation after warm-up
        entt::registry& reg = scene.Registry();
        const BoundingFrustum frustum = BuildFrustum(camera);

        for (auto [entity, transform, renderer] : reg.view<TransformComponent, MeshRendererComponent>().each())
        {
            if (!renderer.visible || !renderer.mesh || !renderer.mesh->IsValid())
                continue;

            ++stats.objectsSubmitted;

            const bool isSky = reg.all_of<SkyboxComponent>(entity);
            XMFLOAT4X4 world = transform.world;
            if (isSky)
            {
                // the sky follows the camera, so it is always "around" the viewer
                world._41 = camera.position.x; world._42 = camera.position.y; world._43 = camera.position.z;
            }

            // ---- world bounding sphere: local sphere moved by the world matrix, radius by the largest axis scale
            const BoundingSphere& local = renderer.mesh->GetBoundingSphere();
            XMFLOAT3 center;
            XMStoreFloat3(&center, XMVector3Transform(XMVectorSet(local.center.x, local.center.y, local.center.z, 1.0f), XMLoadFloat4x4(&world)));
            const float maxScale = std::max({ AxisLength(world._11, world._12, world._13),
                                              AxisLength(world._21, world._22, world._23),
                                              AxisLength(world._31, world._32, world._33) });
            const float radius = local.radius * maxScale;

            if (!isSky && m_Config.frustumCulling)
            {
                DirectX::BoundingSphere sphere(center, radius * m_Config.boundsMargin);
                if (frustum.Contains(sphere) == DISJOINT)
                {
                    ++stats.objectsCulled;
                    continue;
                }
            }

            const float dx = center.x - camera.position.x, dy = center.y - camera.position.y, dz = center.z - camera.position.z;
            const float distance = isSky ? 0.0f : std::sqrt(dx * dx + dy * dy + dz * dz);

            const SkinnedMeshComponent* skin = reg.try_get<SkinnedMeshComponent>(entity);
            const std::vector<XMFLOAT4X4>* bones = (skin && skin->boneMatrices) ? skin->boneMatrices() : nullptr;
            if (bones && bones->empty()) bones = nullptr;

            const size_t submeshCount = std::max<size_t>(1, renderer.mesh->GetSubmeshCount());
            for (size_t si = 0; si < submeshCount; ++si)
            {
                // one material per submesh: the renderer's own entry, else the mesh's
                std::shared_ptr<Material> material =
                    (si < renderer.materials.size() && renderer.materials[si]) ? renderer.materials[si]
                                                                               : renderer.mesh->GetMaterial(si);
                if (!material) continue;

                DrawItem item;
                item.mesh             = renderer.mesh;
                item.material         = material;
                item.submeshIndex     = static_cast<uint32_t>(si);
                item.world            = world;
                item.distanceToCamera = distance;
                item.boneMatrices     = bones;

                if (isSky)                       out.background.push_back(std::move(item));
                else if (IsTransparent(*material)) out.transparent.push_back(std::move(item));
                else                             out.opaque.push_back(std::move(item));
            }
        }

        std::sort(out.opaque.begin(), out.opaque.end(), OpaqueLess);
        std::sort(out.transparent.begin(), out.transparent.end(), TransparentLess);
    }
}
