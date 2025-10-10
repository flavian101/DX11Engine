#include "dxpch.h"
#include "MeshProcessor.h"
#include "Animation/AnimationController.h"

namespace DXEngine
{
	std::shared_ptr<MeshResource> MeshProcessor::ProcessMesh(
		const aiMesh* aiMesh,
		const aiScene* scene,
		std::shared_ptr<Skeleton> skeleton,
		const ModelLoadOptions& options)
	{
		if (!aiMesh || !scene)
		{
			OutputDebugStringA("MeshProcessor: Invalid aiMesh or scene\n");
			return nullptr;
		}

		// FIX: Determine if THIS mesh needs skinning
		bool meshNeedsSkinning = aiMesh->HasBones() && skeleton;

		// Create vertex layout based on mesh properties
		VertexLayout layout = CreateVertexLayout(aiMesh, meshNeedsSkinning, options);

		// Create and setup vertex data
		auto vertexData = std::make_unique<VertexData>(layout);
		vertexData->Resize(aiMesh->mNumVertices);

		// FIX: Only initialize bone data if THIS mesh has bones
		if (meshNeedsSkinning)
		{
			InitializeBoneData(vertexData.get(), aiMesh->mNumVertices);
			// Process bone weights before vertex attributes
			ProcessBoneWeights(aiMesh, vertexData.get(), skeleton);
		}

		// Process vertex attributes (positions, normals, etc)
		ProcessVertexData(aiMesh, vertexData.get(), skeleton);

		// Create index data
		auto indexData = std::make_unique<IndexData>(
			aiMesh->mNumVertices > 65535 ? IndexType::UInt32 : IndexType::UInt16);

		ProcessIndexData(aiMesh, indexData.get());

		// Create mesh resource
		std::string meshName = aiMesh->mName.C_Str();
		if (meshName.empty())
			meshName = "ProcessedMesh_" + std::to_string(m_MeshesProcessed);

		auto meshResource = std::make_unique<MeshResource>(meshName);
		meshResource->SetVertexData(std::move(vertexData));
		meshResource->SetIndexData(std::move(indexData));
		meshResource->SetTopology(PrimitiveTopology::TriangleList);

		// Generate missing data if requested
		if (options.generateNormals && !aiMesh->HasNormals()) {
			meshResource->GenerateNormals();
		}

		if (options.generateTangents && !aiMesh->HasTangentsAndBitangents()) {
			meshResource->GenerateTangents();
		}

		// Generate bounds
		meshResource->GenerateBounds();

		m_MeshesProcessed++;

#ifdef DX_DEBUG
		OutputDebugStringA(("MeshProcessor: Processed mesh '" + meshName +
			"' - Skinned: " + (meshNeedsSkinning ? "Yes" : "No") + "\n").c_str());
#endif

		return meshResource;
	}

	void MeshProcessor::ProcessVertexData(
		const aiMesh* aiMesh,
		VertexData* vertexData,
		std::shared_ptr<Skeleton> skeleton)
	{
		for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
			// Position
			if (aiMesh->HasPositions()) {
				DirectX::XMFLOAT3 pos(
					aiMesh->mVertices[i].x,
					aiMesh->mVertices[i].y,
					aiMesh->mVertices[i].z);
				vertexData->SetAttribute(i, VertexAttributeType::Position, pos);
			}

			// Normal
			if (aiMesh->HasNormals()) {
				DirectX::XMFLOAT3 normal(
					aiMesh->mNormals[i].x,
					aiMesh->mNormals[i].y,
					aiMesh->mNormals[i].z);
				vertexData->SetAttribute(i, VertexAttributeType::Normal, normal);
			}

			// Texture coordinates
			if (aiMesh->HasTextureCoords(0)) {
				DirectX::XMFLOAT2 uv(
					aiMesh->mTextureCoords[0][i].x,
					aiMesh->mTextureCoords[0][i].y);
				vertexData->SetAttribute(i, VertexAttributeType::TexCoord0, uv);
			}

			// Tangents
			if (aiMesh->HasTangentsAndBitangents()) {
				DirectX::XMFLOAT4 tangent(
					aiMesh->mTangents[i].x,
					aiMesh->mTangents[i].y,
					aiMesh->mTangents[i].z,
					1.0f); // Handedness
				vertexData->SetAttribute(i, VertexAttributeType::Tangent, tangent);
			}

			// Additional UV sets
			for (unsigned int uvSet = 1; uvSet < aiMesh->GetNumUVChannels() && uvSet < 4; uvSet++) {
				if (aiMesh->HasTextureCoords(uvSet)) {
					DirectX::XMFLOAT2 uv(
						aiMesh->mTextureCoords[uvSet][i].x,
						aiMesh->mTextureCoords[uvSet][i].y);

					VertexAttributeType uvType = static_cast<VertexAttributeType>(
						static_cast<int>(VertexAttributeType::TexCoord0) + uvSet);

					vertexData->SetAttribute(i, uvType, uv);
				}
			}
		}
	}

	void MeshProcessor::ProcessBoneWeights(
		const aiMesh* aiMesh,
		VertexData* vertexData,
		std::shared_ptr<Skeleton> skeleton)
	{
		if (!aiMesh->HasBones() || !skeleton)
		{
			OutputDebugStringA("MeshProcessor: Invalid aiMesh or skeleton for bone weights\n");
			return;
		}

		// Temporary storage for bone influences per vertex
		struct VertexBoneData {
			std::vector<std::pair<int, float>> influences; // bone index and weight
		};
		std::vector<VertexBoneData> vertexBoneData(aiMesh->mNumVertices);

		// Collect all bone influences
		for (unsigned int boneIndex = 0; boneIndex < aiMesh->mNumBones; boneIndex++)
		{
			const aiBone* bone = aiMesh->mBones[boneIndex];
			std::string boneName = bone->mName.C_Str();

			int skeletonBoneIndex = skeleton->GetBoneIndex(boneName);
			if (skeletonBoneIndex < 0) {
				OutputDebugStringA(("MeshProcessor: Bone not found in skeleton: " +
					boneName + "\n").c_str());
				continue;
			}

			for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++) {
				const aiVertexWeight& weight = bone->mWeights[weightIndex];
				unsigned int vertexId = weight.mVertexId;
				float weightValue = weight.mWeight;

				if (vertexId < aiMesh->mNumVertices && weightValue > 0.0001f) {
					vertexBoneData[vertexId].influences.push_back(
						{ skeletonBoneIndex, weightValue });
				}
			}
		}

		// Assign top 4 influences to each vertex
		int verticesWithoutWeights = 0;
		for (unsigned int i = 0; i < aiMesh->mNumVertices; i++)
		{
			auto& influences = vertexBoneData[i].influences;

			if (influences.empty()) {
				verticesWithoutWeights++;
				continue;
			}

			// Sort by weight (descending)
			std::sort(influences.begin(), influences.end(),
				[](const auto& a, const auto& b) { return a.second > b.second; });

			// Take top 4 and normalize
			DirectX::XMINT4 indices(0, 0, 0, 0);
			DirectX::XMFLOAT4 weights(0.0f, 0.0f, 0.0f, 0.0f);

			float totalWeight = 0.0f;
			size_t count = std::min(influences.size(), size_t(4));

			for (size_t j = 0; j < count; j++) {
				reinterpret_cast<int*>(&indices)[j] = influences[j].first;
				reinterpret_cast<float*>(&weights)[j] = influences[j].second;
				totalWeight += influences[j].second;
			}

			// Normalize weights
			if (totalWeight > 0.0f) {
				weights.x /= totalWeight;
				weights.y /= totalWeight;
				weights.z /= totalWeight;
				weights.w /= totalWeight;
			}

			vertexData->SetAttribute(i, VertexAttributeType::BlendIndices, indices);
			vertexData->SetAttribute(i, VertexAttributeType::BlendWeights, weights);
		}

		if (verticesWithoutWeights > 0) {
			OutputDebugStringA(("MeshProcessor: Warning - " +
				std::to_string(verticesWithoutWeights) +
				" vertices have no bone weights\n").c_str());
		}
	}

	void MeshProcessor::InitializeBoneData(VertexData* vertexData, size_t vertexCount)
	{
		// Initialize all vertices with zero bone data
		DirectX::XMFLOAT4 zeroWeights(0.0f, 0.0f, 0.0f, 0.0f);
		DirectX::XMINT4 zeroIndices(0, 0, 0, 0);

		for (size_t i = 0; i < vertexCount; i++) {
			vertexData->SetAttribute(i, VertexAttributeType::BlendWeights, zeroWeights);
			vertexData->SetAttribute(i, VertexAttributeType::BlendIndices, zeroIndices);
		}
	}

	void MeshProcessor::ProcessIndexData(const aiMesh* aiMesh, IndexData* indexData)
	{
		for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
			const aiFace& face = aiMesh->mFaces[i];

			// Only process triangles
			if (face.mNumIndices == 3) {
				indexData->AddTriangle(
					face.mIndices[0],
					face.mIndices[1],
					face.mIndices[2]);
			}
		}
	}

	VertexLayout MeshProcessor::CreateVertexLayout(
		const aiMesh* aiMesh,
		bool needsSkinning,
		const ModelLoadOptions& options) const
	{
		VertexLayout layout;

		// Position is always required
		layout.Position();

		// Normals
		if (aiMesh->HasNormals() || options.generateNormals) {
			layout.Normal();
		}

		// Texture coordinates
		if (aiMesh->HasTextureCoords(0)) {
			layout.TexCoord(0);
		}

		// Tangents
		if (aiMesh->HasTangentsAndBitangents() || options.generateTangents) {
			layout.Tangent();
		}

		// Additional UV sets
		for (unsigned int i = 1; i < aiMesh->GetNumUVChannels() && i < 4; i++) {
			layout.TexCoord(i);
		}

		// Only add skinning data if THIS mesh actually needs it
		if (needsSkinning) {
			layout.BlendData();
#ifdef DX_DEBUG
			OutputDebugStringA("MeshProcessor: Adding blend data to vertex layout\n");
#endif
		}

		layout.Finalize();
		return layout;
	}
}