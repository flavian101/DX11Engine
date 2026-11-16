#include "dxpch.h"
#include "ModelPostProcessor.h"
#include "models/ModelAsset.h"
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
