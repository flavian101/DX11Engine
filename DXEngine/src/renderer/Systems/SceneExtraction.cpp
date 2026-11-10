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

        // The mesh's local bounding sphere moved by a world matrix; the radius grows with the largest axis scale.
        DirectX::BoundingSphere WorldSphere(const Mesh& mesh, const XMFLOAT4X4& world)
        {
            const BoundingSphere& local = mesh.GetBoundingSphere();
            XMFLOAT3 center;
            XMStoreFloat3(&center, XMVector3Transform(XMVectorSet(local.center.x, local.center.y, local.center.z, 1.0f), XMLoadFloat4x4(&world)));
            const float maxScale = std::max({ AxisLength(world._11, world._12, world._13),
                                              AxisLength(world._21, world._22, world._23),
                                              AxisLength(world._31, world._32, world._33) });
            return DirectX::BoundingSphere(center, local.radius * maxScale);
        }
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
                item.receivesShadows  = renderer.receivesShadows;

                if (isSky)                       out.background.push_back(std::move(item));
                else if (IsTransparent(*material)) out.transparent.push_back(std::move(item));
                else                             out.opaque.push_back(std::move(item));
            }
        }

        std::sort(out.opaque.begin(), out.opaque.end(), OpaqueLess);
        std::sort(out.transparent.begin(), out.transparent.end(), TransparentLess);
    }

    void SceneExtraction::ExtractShadowCasters(Scene& scene, std::vector<DrawItem>& items, std::vector<DirectX::BoundingSphere>& bounds) const
    {
        items.clear();
        bounds.clear();
        entt::registry& reg = scene.Registry();

        for (auto [entity, transform, renderer] : reg.view<TransformComponent, MeshRendererComponent>().each())
        {
            if (!renderer.visible || !renderer.castsShadows || !renderer.mesh || !renderer.mesh->IsValid())
                continue;
            if (reg.all_of<SkyboxComponent>(entity))
                continue;                                                    // the sky is not geometry in the world

            DirectX::BoundingSphere sphere = WorldSphere(*renderer.mesh, transform.world);
            sphere.Radius *= m_Config.boundsMargin;

            const SkinnedMeshComponent* skin = reg.try_get<SkinnedMeshComponent>(entity);
            const std::vector<XMFLOAT4X4>* bones = (skin && skin->boneMatrices) ? skin->boneMatrices() : nullptr;
            if (bones && bones->empty()) bones = nullptr;

            const size_t submeshCount = std::max<size_t>(1, renderer.mesh->GetSubmeshCount());
            for (size_t si = 0; si < submeshCount; ++si)
            {
                std::shared_ptr<Material> material =
                    (si < renderer.materials.size() && renderer.materials[si]) ? renderer.materials[si]
                                                                               : renderer.mesh->GetMaterial(si);
                if (!material || IsTransparent(*material))
                    continue;                                                // glass / particles do not cast (yet)

                DrawItem item;
                item.mesh         = renderer.mesh;
                item.material     = material;                                // only its cull mode matters to the shadow pass
                item.submeshIndex = static_cast<uint32_t>(si);
                item.world        = transform.world;
                item.boneMatrices = bones;
                items.push_back(std::move(item));
                bounds.push_back(sphere);
            }
        }

        // sort by mesh (keeping items and bounds together) so consecutive draws share vertex/index buffers
        std::vector<uint32_t> order(items.size());
        for (uint32_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b)
        {
            if (items[a].mesh != items[b].mesh) return std::less<const Mesh*>{}(items[a].mesh.get(), items[b].mesh.get());
            return a < b;
        });
        std::vector<DrawItem> sortedItems; sortedItems.reserve(items.size());
        std::vector<DirectX::BoundingSphere> sortedBounds; sortedBounds.reserve(items.size());
        for (uint32_t i : order) { sortedItems.push_back(std::move(items[i])); sortedBounds.push_back(bounds[i]); }
        items.swap(sortedItems);
        bounds.swap(sortedBounds);
    }
}
