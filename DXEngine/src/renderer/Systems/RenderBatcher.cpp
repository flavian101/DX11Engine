#include "dxpch.h"
#include "RenderBatcher.h"
#include "camera/Camera.h"
#include "models/Model.h"
#include "models/ModelFeatures.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace DXEngine
{
    // ---- sort policies: ONE place to change the batching strategy ----

    // Opaque: group by material, then mesh (fewer state changes), then near-to-far inside a group.
    // Trade-off: exact front-to-back would cut overdraw more, but costs more state changes.
    // Swap the first two comparisons to try it.
    static bool OpaqueLess(const DrawItem& a, const DrawItem& b)
    {
        if (a.material != b.material)
            return std::less<const Material*>{}(a.material.get(), b.material.get());
        if (a.mesh != b.mesh)
            return std::less<const Mesh*>{}(a.mesh.get(), b.mesh.get());
        return a.distanceToCamera < b.distanceToCamera;
    }

    // Transparent: strictly far-to-near, otherwise blending looks wrong.
    static bool TransparentLess(const DrawItem& a, const DrawItem& b)
    {
        return a.distanceToCamera > b.distanceToCamera;
    }

    void RenderBatcher::BuildBatches(FrameContext& ctx)
    {
        ctx.batches.Clear();                       // keeps capacity
        if (!ctx.camera) return;

        const DirectX::XMFLOAT3 camPos = ctx.camera->GetPositionFloat3();

        for (const SceneModel* entry : ctx.visibility.visibleModels)
        {
            if (!entry || !entry->model) continue;   // was `return` before: one null model dropped the WHOLE frame
            AppendModel(*entry, camPos, ctx.batches);
        }

        std::sort(ctx.batches.opaque.begin(), ctx.batches.opaque.end(), OpaqueLess);
        std::sort(ctx.batches.transparent.begin(), ctx.batches.transparent.end(), TransparentLess);
    }

    bool RenderBatcher::IsTransparent(const Material& m)
    {
        return m.GetType() == MaterialType::Transparent || m.GetBlendMode() != RHI::BlendMode::Opaque;
    }

    void RenderBatcher::AppendModel(const SceneModel& entry, const DirectX::XMFLOAT3& camPos, BatchQueue& out) const
    {
        const Model* model = entry.model.get();

        DirectX::XMFLOAT4X4 world;
        DirectX::XMStoreFloat4x4(&world, model->GetModelMatrix());   // computed ONCE per model

        const BoundingSphere s = model->GetWorldBoundingSphere();
        const float dx = s.center.x - camPos.x, dy = s.center.y - camPos.y, dz = s.center.z - camPos.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        const InstanceData* instances = model->IsInstanced() ? model->GetInstanceData() : nullptr;
        const SkinningData* skin      = model->IsSkinned()   ? model->GetSkinningData() : nullptr;

        for (size_t mi = 0; mi < model->GetMeshCount(); ++mi)
        {
            std::shared_ptr<Mesh> mesh = model->GetMesh(mi);
            if (!mesh || !mesh->IsValid()) continue;

            const size_t submeshCount = std::max<size_t>(1, mesh->GetSubmeshCount());
            for (size_t si = 0; si < submeshCount; ++si)
            {
                // material resolution happens HERE, so passes never see overrides or defaults
                std::shared_ptr<Material> material = entry.materialOverride ? entry.materialOverride
                                                                            : model->GetMaterial(mi, si);
                if (!material) continue;

                DrawItem item;
                item.mesh             = mesh;
                item.material        = material;
                item.submeshIndex    = static_cast<uint32_t>(si);
                item.world           = world;
                item.distanceToCamera = distance;
                if (instances && instances->GetInstanceCount() > 0) item.instanceTransforms = &instances->transforms;
                if (skin && !skin->boneMatrices.empty())            item.boneMatrices       = &skin->boneMatrices;

                if (material->GetType() == MaterialType::Skybox)
                {
                    // A skybox follows the camera. Doing it here keeps every pass generic.
                    item.world._41 = camPos.x; item.world._42 = camPos.y; item.world._43 = camPos.z;
                    out.background.push_back(std::move(item));
                }
                else if (IsTransparent(*material))
                    out.transparent.push_back(std::move(item));
                else
                    out.opaque.push_back(std::move(item));
            }
        }
    }
}
