#pragma once
#include "utils/Mesh/Mesh.h"
#include "utils/ConstantBufferTypes.h"
#include "Resource/SkinnedMeshResource.h"

namespace DXEngine
{
    class RawBuffer;
    class Skeleton;

    class SkinnedMesh : public Mesh
    {
    public:
        explicit SkinnedMesh(std::shared_ptr<SkinnedMeshResource> resource);

        const std::shared_ptr<SkinnedMeshResource>& GetSkinnedResource() const
        {
            return std::dynamic_pointer_cast<SkinnedMeshResource>(GetResource());
        }

        // Bone matrices for animation
        void SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices);
        const std::vector<DirectX::XMFLOAT4X4>& GetBoneMatrices() const { return m_BoneMatrices; }

        //Get the Skeleton
        std::shared_ptr<Skeleton> GetSkeleton() const;
        // Bind bone data for rendering
        void BindBoneData() const;

        void Bind(const void* shaderByteCode = nullptr, size_t byteCodeLength = 0) const;
        bool HasSkinningData()const;
        static constexpr size_t GetMaxBones() { return 128; } // to be set

    protected:
        void OnResourceChanged()override;

    private:
        void CreateBoneBuffer();
        void UpdateBoneBuffer();


    private:
        std::vector<DirectX::XMFLOAT4X4> m_BoneMatrices;
        std::unique_ptr<RawBuffer> m_BoneBuffer;
        mutable bool m_BoneBufferDirty = true;
    };
}
