#pragma once
#include <memory>
#include <assimp/scene.h>
#include <DirectXMath.h>

namespace DXEngine
{
	class Skeleton;
	class SkeletonProcessor
	{
	public:
		SkeletonProcessor() = default;

		std::shared_ptr<Skeleton> ProcessSkeleton(const aiScene* scene, const aiMesh* mesh);

		void ProcessNodeHierarchy(
			const aiNode* node,
			const aiScene* scene,
			std::shared_ptr<Skeleton> skeleton,
			int parentIndex,
			std::unordered_map<std::string, int>& boneMapping
		);

		bool ValidateSkeleton(std::shared_ptr<Skeleton> skeleton) const;
		bool ValidateBoneHierarchy(std::shared_ptr<Skeleton> skeleton, int boneIndex, std::vector<bool>& visited)const;
		DirectX::XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& matrix);

		size_t GetBonesProcessed() const { return m_BonesProcessed; }
		void ResetStats() { m_BonesProcessed = 0; }

	private:
		size_t m_BonesProcessed = 0;
	};
}
