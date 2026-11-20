#include "dxpch.h"
#include "ModelPostProcessor.h"
#include "models/ModelAsset.h"
#include "Animation/AnimationEvaluator.h"
#include "utils/Mesh/Utils/SkinnedBounds.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"

namespace DXEngine {

    void ModelPostProcessor::PostProcess(const std::shared_ptr<ModelAsset>& asset, const ModelLoadOptions& options)
    {
        if (!asset)
            return;

        EnsureDefaultMaterials(asset);

        if (options.globalScale != 1.0f)
            ApplyGlobalScale(asset, options.globalScale);

        if (options.optimizeMeshes)
            OptimizeModel(asset);

        UpdateSkinnedBounds(asset);                        // after the optimisation: it reorders the vertices

        if (options.validateDataStructure)
            ValidateModel(asset);
    }

    void ModelPostProcessor::EnsureDefaultMaterials(const std::shared_ptr<ModelAsset>& asset)
    {
        if (!asset)
            return;

        for (size_t i = 0; i < asset->meshes.size(); i++) {
            auto& mesh = asset->meshes[i].mesh;
            if (!mesh || !mesh->IsValid())
                continue;

            // a submesh without a material gets a plain lit one (and is listed with the asset's materials)
            const size_t submeshCount = std::max(size_t(1), mesh->GetSubmeshCount());
            for (size_t j = 0; j < submeshCount; j++) {
                if (!mesh->HasMaterial(j)) {
                    auto defaultMaterial = MaterialFactory::CreateLitMaterial(
                        "DefaultMaterial_" + std::to_string(i) + "_" + std::to_string(j));
                    mesh->SetMaterial(j, defaultMaterial);
                    asset->materials.push_back(std::move(defaultMaterial));
                }
            }
        }
    }

    void ModelPostProcessor::OptimizeModel(const std::shared_ptr<ModelAsset>& asset)
    {
        if (!asset)
            return;

        size_t reordered = 0;
        for (ModelMesh& entry : asset->meshes) {
            if (entry.mesh && entry.mesh->IsValid() && entry.mesh->OptimizeForRendering())
                ++reordered;
        }

        OutputDebugStringA(("ModelPostProcessor: vertex cache and fetch order improved for " +
            std::to_string(reordered) + " of " + std::to_string(asset->meshes.size()) + " meshes\n").c_str());
    }

    void ModelPostProcessor::UpdateSkinnedBounds(const std::shared_ptr<ModelAsset>& asset)
    {
        if (!asset || !asset->IsSkinned())
            return;

        // the poses a mesh can take: the bind pose (a clip without channels) and 16 samples across every clip
        std::vector<std::vector<DirectX::XMFLOAT4X4>> poses;
        AnimationEvaluator evaluator;
        {
            AnimationClip bind("bind", 1.0f);
            poses.emplace_back();
            evaluator.Evaluate(bind, *asset->skeleton, 0.0f, poses.back());
        }
        constexpr int kSamplesPerClip = 16;
        for (const auto& clip : asset->clips)
        {
            if (!clip || clip->GetDurationSeconds() <= 0.0f) continue;
            for (int s = 0; s <= kSamplesPerClip; ++s)
            {
                poses.emplace_back();
                evaluator.Evaluate(*clip, *asset->skeleton, clip->GetDurationSeconds() * static_cast<float>(s) / kSamplesPerClip, poses.back());
            }
        }

        for (const ModelMesh& entry : asset->meshes)
        {
            if (!entry.skinned || !entry.mesh || !entry.mesh->GetResource() || !entry.mesh->GetResource()->GetVertexData())
                continue;
            auto& resource = *entry.mesh->GetResource();
            const VertexData& vertices = *resource.GetVertexData();

            std::vector<DirectX::XMFLOAT3> positions;
            std::vector<DirectX::XMINT4>   indices;
            std::vector<DirectX::XMFLOAT4> weights;
            const size_t count = vertices.GetVertexCount();
            positions.reserve(count); indices.reserve(count); weights.reserve(count);
            for (size_t v = 0; v < count; ++v)
            {
                positions.push_back(vertices.GetAttribute<DirectX::XMFLOAT3>(v, RHI::VertexAttributeType::Position));
                indices.push_back(vertices.GetAttribute<DirectX::XMINT4>(v, RHI::VertexAttributeType::BlendIndices));
                weights.push_back(vertices.GetAttribute<DirectX::XMFLOAT4>(v, RHI::VertexAttributeType::BlendWeights));
            }

            const SkinnedBounds::Box box = SkinnedBounds::Compute(positions, indices, weights, poses);
            if (!box.valid())
            {
                asset->warnings.push_back("Mesh '" + entry.name + "': the bounding box of the skinned mesh could not be computed; the raw vertex box is used");
                continue;
            }

            const DirectX::XMFLOAT3 center = { (box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f, (box.min.z + box.max.z) * 0.5f };
            const float radius = 0.5f * std::sqrt((box.max.x - box.min.x) * (box.max.x - box.min.x) +
                                                  (box.max.y - box.min.y) * (box.max.y - box.min.y) +
                                                  (box.max.z - box.min.z) * (box.max.z - box.min.z));
            resource.SetBounds(BoundingBox(box.min, box.max), BoundingSphere(center, radius));
        }
    }

    void ModelPostProcessor::ValidateModel(const std::shared_ptr<ModelAsset>& asset)
    {
        if (!asset)
            return;

        for (const std::string& problem : asset->Validate())
            asset->warnings.push_back("Validation: " + problem);

        for (size_t i = 0; i < asset->meshes.size(); i++) {
            const auto& mesh = asset->meshes[i].mesh;
            if (!mesh || !mesh->GetResource())
                continue;                                  // Validate() already reported it
            std::string errorMessage;
            if (!MeshUtils::ValidateMesh(*mesh->GetResource(), errorMessage))
                asset->warnings.push_back("Mesh '" + asset->meshes[i].name + "' failed validation: " + errorMessage);
        }

        if (asset->IsSkinned() && asset->clips.empty())
            asset->warnings.push_back("The model is skinned but the file has no animation: it stays in its bind pose");
        if (!asset->IsSkinned() && !asset->clips.empty())
            asset->warnings.push_back("The file has animation but no skinned mesh: the clips were ignored");

        for (size_t i = 0; i < asset->clips.size(); i++) {
            if (asset->clips[i] && asset->clips[i]->GetDuration() <= 0.0f)
                asset->warnings.push_back("Animation '" + asset->clips[i]->GetName() + "' has zero duration");
        }
    }

    void ModelPostProcessor::ApplyGlobalScale(const std::shared_ptr<ModelAsset>& asset, float scale)
    {
        if (asset)
            asset->ApplyScale(scale);
    }
}
