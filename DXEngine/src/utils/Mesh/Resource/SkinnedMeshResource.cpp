#include "dxpch.h"
#include "SkinnedMeshResource.h"

namespace DXEngine
{
    void SkinnedMeshResource::AddBone(const BoneInfo& bone)
    {
        m_Bones.push_back(bone);
    }

    int32_t SkinnedMeshResource::FindBoneIndex(const std::string& name) const
    {
        for (size_t i = 0; i < m_Bones.size(); ++i)
        {
            if (m_Bones[i].name == name)
                return static_cast<int32_t>(i);
        }
        return -1;
    }

}