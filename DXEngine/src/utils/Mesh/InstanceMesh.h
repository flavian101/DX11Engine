#pragma once
#include "Utils/Mesh/Mesh.h"
#include "Utils/Buffer.h"

namespace DXEngine
{
    class InstancedMeshResource;

    class InstancedMesh : public Mesh
    {
    public:
        explicit InstancedMesh(std::shared_ptr<InstancedMeshResource> resource);

        const std::shared_ptr<InstancedMeshResource>& GetInstancedResource() const
        {
            return std::static_pointer_cast<InstancedMeshResource>(GetResource());
        }

        // Instance rendering
        void DrawInstanced(size_t submeshIndex = 0) const;
        void DrawAllInstanced() const;

        // Update instance data on GPU
        bool UpdateInstanceData() const;

    protected:
        void OnResourceChanged() override;

    private:
        mutable std::unique_ptr<RawBuffer> m_InstanceBuffer;
        mutable bool m_InstanceDataDirty = true;
    };

}

