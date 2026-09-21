#pragma once
#include "models/Model.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include "../Cache/ResourceCache.h"
#include "../Core/FrameContext.h"
#include <vector>
#include <memory>

namespace DXEngine {
	//Draw Item 
	//a single resolved draw call ready for the GPU
	//passes consume these- they never touch Model Directly

	struct DrawItem
	{
		//Geometry
		std::shared_ptr<Mesh> mesh;

		size_t meshIndex = 0;
		size_t submeshIndex = 0;

		//material(effective = override ?? mesh material)
		std::shared_ptr<Material> material;

		//Transform (pre-computed from Model::GetModelMatrix
		DirectX::XMFLOAT4X4 modelMatrix;
		DirectX::XMFLOAT4X4 normalMatrix;

		//Instancing (non-null when model ->isInstanced)
		const std::vector<DirectX::XMFLOAT4X4>* instanceTransforms = nullptr;
		uint32_t instanceCount = 1;

		//skinning (non-null when model->isSkinned)
		const std::vector<DirectX::XMFLOAT4X4>* boneMatrices = nullptr;

		//sort Key (set by batcher, used by passes)
		float distanceToCamera = 0.0f;
		uint64_t stateKey = 0;


		bool castsShadow = true;
		bool receivesShadows = true;

		//helpers
		bool IsInstanced() const { return instanceTransforms && instanceCount > 1; }
		bool IsSkinned()   const { return boneMatrices && !boneMatrices->empty(); }
	};


	//Batch queue
	//A sorted, ready-to-execute list of drawItems for one render pass
	struct BatchQueue
	{
		std::vector<DrawItem> opaque;       // Sorted front-to-back
		std::vector<DrawItem> transparent;  // Sorted back-to-front
		std::vector<DrawItem> ui;           // Submission order
		std::vector<DrawItem> shadowCasters;// Unsorted (shadow pass sorts if needed)
	};


	//Render Batcher
	//Single Responsibility: turn visible models into sorted DrawItems
	//Reads from FrameContext:Visibility
	//Writes: BatchQueue int frameContext
	class RenderBatcher
	{
	public:
		RenderBatcher() = default;

		//MainEntry point
		//calls after Culling System::cull, before pass execution
		//result stored in ctx under key "BatchQueue"
		void BuildBatches(FrameContext& ctx);


		//Material Override
		//set before buildBatches- applied to all draws this frame
		//cleared automatically each frame
		void SetGlobalMaterialOverride(std::shared_ptr<Material> mat) { m_GlobalOverride = mat; }
		

	private:
		//build one drawItem per (mesh+ submesh) from model
		//Material is resolved here so passes
		// never need to know about overrides or defaults.
		void BuildDrawItems(
			const Model* model,
			std::shared_ptr<Material> matOverride,
			float distanceToCamera,
			std::vector<DrawItem>& outOpaque,
			std::vector<DrawItem>& outTransparent
		);

		//sort helpers
		void SortFrontToBack(std::vector<DrawItem>& items) const;
		void SortBackToFront(std::vector<DrawItem>& items) const;

		//state Key: material ptr | mesh ptr - minimizes GPU state changes
		uint64_t BuildStateKey(const Material* mat, const Mesh* mesh)const;

		//Queue classification from material type
		bool IsTransparent(const Material* mat) const;




	private:
		std::shared_ptr<Material> m_GlobalOverride;

	};

}

