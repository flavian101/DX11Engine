#pragma once
#include <memory>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include "ModelLoaderUtils.h"
#include "utils/Mesh/Mesh.h"


namespace DXEngine
{
	class MeshResource;
	class Skeleton;

	class MeshProcessor
	{
	public:
		MeshProcessor() = default;
		std::shared_ptr<MeshResource> ProcessMesh(
			const aiMesh* aiMesh,
			const aiScene* scene,
			std::shared_ptr<Skeleton> skeleton, 
			const ModelLoadOptions& options);

		size_t GetMeshesProcessed() const { return m_MeshesProcessed; }
		void ResetStats() { m_MeshesProcessed = 0; }

	private:
        void ProcessVertexData(
            const aiMesh* aiMesh,
            VertexData* vertexData,
            std::shared_ptr<Skeleton> skeleton);

        void ProcessBoneWeights(
            const aiMesh* aiMesh,
            VertexData* vertexData,
            std::shared_ptr<Skeleton> skeleton);

        void InitializeBoneData(VertexData* vertexData, size_t vertexCount);

        // Index data processing
        void ProcessIndexData(const aiMesh* aiMesh, IndexData* indexData);

        // Layout creation
		VertexLayout CreateVertexLayout(
			const aiMesh* aiMesh,
			bool needsSkinning,
			const ModelLoadOptions& options) const;

	private:
		size_t m_MeshesProcessed = 0;
	};
}
