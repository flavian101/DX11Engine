#pragma once
#include "utils/Mesh/Resource/MeshResource.h"


namespace DXEngine
{
	class SkinnedMeshResource : public MeshResource
	{
	public:
		struct BoneInfo
		{
			std::string name;
			DirectX::XMFLOAT4X4 offsetMatrix;
			int32_t parentIndex = -1;
		};

		SkinnedMeshResource(const std::string& name = "SkinnedMesh") : MeshResource(name) {}

		// Bone management
		void AddBone(const BoneInfo& bone);
		const std::vector<BoneInfo>& GetBones() const { return m_Bones; }
		BoneInfo& GetBone(size_t index) { return m_Bones[index]; }
		const BoneInfo& GetBone(size_t index) const { return m_Bones[index]; }
		size_t GetBoneCount() const { return m_Bones.size(); }

		int32_t FindBoneIndex(const std::string& name) const;

	private:
		std::vector<BoneInfo> m_Bones;
	};
}

