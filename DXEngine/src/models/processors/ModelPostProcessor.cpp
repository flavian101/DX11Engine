#include "dxpch.h"
#include "ModelPostProcessor.h"
#include "models/Model.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"

namespace DXEngine {

    void ModelPostProcessor::PostProcess(
        std::shared_ptr<Model> model,
        const ModelLoadOptions& options)
    {
        if (!model) {
            m_LastError = "Null model provided";
            return;
        }

        // Ensure default materials for all meshes
        EnsureDefaultMaterials(model);

        // Apply global scale if needed
        if (options.globalScale != 1.0f) {
            ApplyGlobalScale(model, options.globalScale);
        }

        // Optimize if requested
        if (options.optimizeMeshes) {
            OptimizeModel(model);
        }

        // Validate if requested
        if (options.validateDataStructure) {
            ValidateModel(model);
        }
    }

    void ModelPostProcessor::EnsureDefaultMaterials(std::shared_ptr<Model> model)
    {
        if (!model)
            return;

        for (size_t i = 0; i < model->GetMeshCount(); i++) {
            auto mesh = model->GetMesh(i);
            if (!mesh || !mesh->IsValid())
                continue;

            // Check if any submesh needs a default material
            size_t submeshCount = std::max(size_t(1), mesh->GetSubmeshCount());
            for (size_t j = 0; j < submeshCount; j++) {
                if (!mesh->HasMaterial(j)) {
                    auto defaultMaterial = MaterialFactory::CreateLitMaterial(
                        "DefaultMaterial_" + std::to_string(i) + "_" + std::to_string(j));
                    mesh->SetMaterial(j, defaultMaterial);

#ifdef DX_DEBUG
                    OutputDebugStringA(("ModelPostProcessor: Added default material for mesh " +
                        std::to_string(i) + " submesh " + std::to_string(j) + "\n").c_str());
#endif
                }
            }
        }
    }

    void ModelPostProcessor::OptimizeModel(std::shared_ptr<Model> model)
    {
        if (!model)
            return;

        OutputDebugStringA("ModelPostProcessor: Optimizing model...\n");

        for (size_t i = 0; i < model->GetMeshCount(); i++) {
            auto mesh = model->GetMesh(i);
            if (!mesh || !mesh->IsValid())
                continue;

            auto meshResource = mesh->GetResource();
            if (meshResource) {
                // Optimize index order for better cache utilization
              //  meshResource->OptimizeForRendering();
            }
        }

        OutputDebugStringA("ModelPostProcessor: Optimization complete\n");
    }

    void ModelPostProcessor::ValidateModel(std::shared_ptr<Model> model)
    {
        if (!model) {
            m_LastError = "Null model";
            return;
        }

        if (!model->IsValid()) {
            m_LastError = "Model validation failed: Invalid model";
            return;
        }

        // Validate each mesh
        for (size_t i = 0; i < model->GetMeshCount(); i++) {
            auto mesh = model->GetMesh(i);
            if (!mesh || !mesh->IsValid()) {
                m_LastError = "Model validation failed: Invalid mesh at index " + std::to_string(i);
                return;
            }

            auto meshResource = mesh->GetResource();
            if (meshResource) {
                std::string errorMessage;
                if (!MeshUtils::ValidateMesh(*meshResource, errorMessage)) {
                    m_LastError = "Mesh validation failed: " + errorMessage;
                    return;
                }
            }
        }

        // Validate skinning if present
        if (model->IsSkinned()) {
            auto skeleton = model->GetSkeleton();
            if (!skeleton) {
                m_LastError = "Model is marked as skinned but has no skeleton";
                return;
            }

            if (skeleton->GetBoneCount() == 0) {
                m_LastError = "Skeleton has no bones";
                return;
            }

            // Validate bone matrices
            const auto& boneMatrices = model->GetBoneMatrices();
            if (boneMatrices.size() != skeleton->GetBoneCount()) {
                m_LastError = "Bone matrix count mismatch: " +
                    std::to_string(boneMatrices.size()) + " vs " +
                    std::to_string(skeleton->GetBoneCount());
                return;
            }
        }

        // Validate animations if present
        if (model->GetAnimationClipCount() > 0) {
            if (!model->IsSkinned()) {
                OutputDebugStringA("ModelPostProcessor: Warning - Model has animations but no skeleton\n");
            }

            for (size_t i = 0; i < model->GetAnimationClipCount(); i++) {
                auto clip = model->GetAnimationClip(i);
                if (!clip) {
                    m_LastError = "Invalid animation clip at index " + std::to_string(i);
                    return;
                }

                if (clip->GetDuration() <= 0.0f) {
                    OutputDebugStringA(("ModelPostProcessor: Warning - Animation '" +
                        clip->GetName() + "' has zero duration\n").c_str());
                }
            }
        }

        OutputDebugStringA("ModelPostProcessor: Validation successful\n");
        m_LastError.clear();
    }

    void ModelPostProcessor::ApplyGlobalScale(std::shared_ptr<Model> model, float scale)
    {
        if (!model || scale == 1.0f)
            return;

        DirectX::XMFLOAT3 scaleVec(scale, scale, scale);
        model->SetScale(scaleVec);

        OutputDebugStringA(("ModelPostProcessor: Applied global scale: " +
            std::to_string(scale) + "\n").c_str());
    }

} // namespace DXEngine