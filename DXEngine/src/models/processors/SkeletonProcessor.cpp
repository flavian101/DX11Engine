#include "dxpch.h"
#include "SkeletonProcessor.h"
#include "Animation/AnimationController.h"
#include "Animation/AnimationClip.h"

namespace DXEngine
{
	std::shared_ptr<Skeleton> SkeletonProcessor::ProcessSkeleton(const aiScene* scene, const aiMesh* mesh)
	{
		if (!mesh || !mesh->HasBones() || !scene)
		{
			OutputDebugStringA("SkeletonProcessor: Empty aiScene or emtpy aiMesh");
			return nullptr;
		}

		auto skeleton = std::make_shared<Skeleton>();
		std::unordered_map<std::string, int> boneMapping;

		// ========================================================================
		// PASS 1: Create bones from mesh bone data (get offset matrices)
		// ========================================================================
		for (unsigned int i = 0; i < mesh->mNumBones; i++)
		{
			const aiBone* aiBone = mesh->mBones[i];
			std::string boneName = aiBone->mName.C_Str();

			Bone bone;
			bone.Name = boneName;
			bone.OffsetMatrix = ConvertMatrix(aiBone->mOffsetMatrix);
			bone.ParentIndex = -1; // to be set in the hierarchy pass

			//initialize local transforms to identity
			DirectX::XMStoreFloat4x4(&bone.LocalTransform, DirectX::XMMatrixIdentity());

			//add bone to skeleton and store mapping
			boneMapping[boneName] = static_cast<int>(skeleton->GetBoneCount());
			skeleton->AddBone(bone);	
		}
		// ========================================================================
		// PASS 2: Build hierarchy from scene node structure/
		// ========================================================================
		if (scene->mRootNode)
		{
			ProcessNodeHierarchy(scene->mRootNode, scene, skeleton, -1, boneMapping);
		}

		// ========================================================================
		// PASS 3: Validate skeleton
	    // ========================================================================
		if (!ValidateSkeleton(skeleton))
		{
			OutputDebugStringA("SkeletonProcessor: Skeleton validation failed\n");
			return nullptr;
		}

		m_BonesProcessed += static_cast<uint32_t>(skeleton->GetBoneCount());

		OutputDebugStringA(("SkeletonProcessor: Created skeleton with " +
			std::to_string(skeleton->GetBoneCount()) + " bones\n").c_str());

		return skeleton;
	}
	void SkeletonProcessor::ProcessNodeHierarchy(const aiNode* node, const aiScene* scene, std::shared_ptr<Skeleton> skeleton, int parentIndex, std::unordered_map<std::string, int>& boneMapping)
	{
		if (!node || !skeleton)
		{
			OutputDebugStringA("SkeletonProcessor: null node or skeleton");
			return;
		}

		std::string nodeName = node->mName.C_Str();
		int currentBoneIndex = -1;

		//check if this node corresponds to a bone
		auto it = boneMapping.find(nodeName);
		if (it != boneMapping.end())
		{
			currentBoneIndex = it->second;
			Bone& bone = skeleton->GetBone(currentBoneIndex);

			//set Parent index
			bone.ParentIndex = parentIndex;

			// Set local transform from node transformation
			bone.LocalTransform = ConvertMatrix(node->mTransformation);

			// Use this bone as parent for children
			parentIndex = currentBoneIndex;

#ifdef DX_DEBUG
			OutputDebugStringA(("  Bone: " + nodeName +
				" (Index: " + std::to_string(currentBoneIndex) +
				", Parent: " + std::to_string(bone.ParentIndex) + ")\n").c_str());
#endif
		}

		//Recursivley process children
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			ProcessNodeHierarchy(node->mChildren[i], scene, skeleton, parentIndex, boneMapping);
		}
	}
	bool SkeletonProcessor::ValidateSkeleton(std::shared_ptr<Skeleton> skeleton) const
	{
		if (!skeleton)
		{
			OutputDebugStringA("ValidateSkeleton: skeleton is NULL\n");
			return false;
		}

		if (skeleton->GetBoneCount() == 0)
		{
			OutputDebugStringA("ValidateSkeleton: skeleton has 0 bones\n");
			return false;
		}

		OutputDebugStringA(("Validating skeleton with " +
			std::to_string(skeleton->GetBoneCount()) + " bones...\n").c_str());

		// Check for circular dependencies
		std::vector<bool> visited(skeleton->GetBoneCount(), false);

		for (size_t i = 0; i < skeleton->GetBoneCount(); i++)
		{
			// Reset visited for each bone chain check
			std::fill(visited.begin(), visited.end(), false);

			if (!ValidateBoneHierarchy(skeleton, i, visited))
			{
				OutputDebugStringA(("ValidateSkeleton: Invalid hierarchy at bone " +
					std::to_string(i) + " (" + skeleton->GetBone(i).Name + ")\n").c_str());
				return false;
			}
		}

		// Check for orphaned bones (except root)
		int rootCount = 0;
		for (size_t i = 0; i < skeleton->GetBoneCount(); i++)
		{
			const Bone& bone = skeleton->GetBone(i);
			if (bone.ParentIndex == -1)
			{
				rootCount++;
#ifdef DX_DEBUG
				OutputDebugStringA(("  Root bone found: " + bone.Name + "\n").c_str());
#endif
			}
		}

		if (rootCount == 0)
		{
			OutputDebugStringA("ValidateSkeleton: No root bone found\n");
			return false;
		}

		if (rootCount > 1)
		{
			OutputDebugStringA(("ValidateSkeleton: Multiple root bones found (" +
				std::to_string(rootCount) + ") - this is acceptable\n").c_str());
		}

		OutputDebugStringA("Skeleton validation passed\n");
		return true;
	}
	bool SkeletonProcessor::ValidateBoneHierarchy(std::shared_ptr<Skeleton> skeleton, int boneIndex, std::vector<bool>& visited) const
	{
		// Out of bounds check
		if (boneIndex < 0 || boneIndex >= static_cast<int>(skeleton->GetBoneCount()))
			return true;  // End of chain

		// Circular dependency check
		if (visited[boneIndex])
		{
			OutputDebugStringA(("Circular dependency detected at bone " +
				std::to_string(boneIndex) + " (" +
				skeleton->GetBone(boneIndex).Name + ")\n").c_str());
			return false;
		}

		visited[boneIndex] = true;

		const Bone& bone = skeleton->GetBone(boneIndex);

		// Validate parent
		if (bone.ParentIndex >= 0)
		{
			if (bone.ParentIndex >= static_cast<int>(skeleton->GetBoneCount()))
			{
				OutputDebugStringA(("Invalid parent index " +
					std::to_string(bone.ParentIndex) + " for bone " +
					std::to_string(boneIndex) + " (" + bone.Name + ")\n").c_str());
				return false;
			}

			// Check parent hierarchy
			return ValidateBoneHierarchy(skeleton, bone.ParentIndex, visited);
		}

		return true;
	}
	DirectX::XMFLOAT4X4 SkeletonProcessor::ConvertMatrix(const aiMatrix4x4& matrix)
	{
		return DirectX::XMFLOAT4X4(
			matrix.a1, matrix.b1, matrix.c1, matrix.d1,
			matrix.a2, matrix.b2, matrix.c2, matrix.d2,
			matrix.a3, matrix.b3, matrix.c3, matrix.d3,
			matrix.a4, matrix.b4, matrix.c4, matrix.d4
		);
	}
}
