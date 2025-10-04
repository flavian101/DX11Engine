#pragma once
#include "utils/Mesh/Resource/MeshResource.h"
#include "Animation/AnimationClip.h" // Use the animation system's Skeleton

namespace DXEngine
{
    class SkinnedMeshResource : public MeshResource
    {
    public:
        SkinnedMeshResource(const std::string& name = "SkinnedMesh") : MeshResource(name) {}

        // Set/Get the skeleton reference
        void SetSkeleton(std::shared_ptr<Skeleton> skeleton) { m_Skeleton = skeleton; }
        const std::shared_ptr<Skeleton>& GetSkeleton() const { return m_Skeleton; }
        std::shared_ptr<Skeleton> GetSkeleton() { return m_Skeleton; }

        // Helper to validate bone weights in vertex data
        bool ValidateBoneWeights() const;

        std::string GetDebugInfo() const
        {
            std::string info = MeshResource::GetDebugInfo();
            if (m_Skeleton)
            {
                info += "Skeleton Bones: " + std::to_string(m_Skeleton->GetBoneCount()) + "\n";
            }
            return info;
        }

    private:
        std::shared_ptr<Skeleton> m_Skeleton;
    };
}