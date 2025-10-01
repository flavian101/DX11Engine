#pragma once
#include "utils/Mesh/Mesh.h"

namespace DXEngine
{
    class SkinnedMeshResource;
    class RawBuffer;

    class SkinnedMesh : public Mesh
    {
    public:
        explicit SkinnedMesh(std::shared_ptr<SkinnedMeshResource> resource);

        const std::shared_ptr<SkinnedMeshResource>& GetSkinnedResource() const
        {
            return std::static_pointer_cast<SkinnedMeshResource>(GetResource());
        }

        // Bone matrices for animation
        void SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices);
        const std::vector<DirectX::XMFLOAT4X4>& GetBoneMatrices() const { return m_BoneMatrices; }

        // Bind bone data for rendering
        void BindBoneData() const;

    private:
        std::vector<DirectX::XMFLOAT4X4> m_BoneMatrices;
        std::unique_ptr<RawBuffer> m_BoneBuffer;
    };
}
