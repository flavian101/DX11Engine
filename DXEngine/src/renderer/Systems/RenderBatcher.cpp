#include "dxpch.h"
#include "RenderBatcher.h"
#include <algorithm>
#include "../../camera/Camera.h"


namespace DXEngine {
	void RenderBatcher::BuildBatches(FrameContext& ctx)
	{
		//start with a fresh queue each frame
		BatchQueue queue;

		if (!ctx.camera)
			return;

		DirectX::XMFLOAT3 camPos = ctx.camera->GetPositionFloat3();
		DirectX::XMVECTOR camVec = DirectX::XMLoadFloat3(&camPos);


		//visible Opaque/ transparent geometry
		for (const Model* model : ctx.visibility.visibleModels)
		{
			if (!model)
				return;

			//Distance to the camera(Sphere center)

			BoundingSphere sphere = model->GetWorldBoundingSphere();
			DirectX::XMVECTOR center = DirectX::XMLoadFloat3(&sphere.center);
			float dist =DirectX::XMVectorGetX(
				DirectX::XMVector3Length(DirectX::XMVectorSubtract(center, camVec)));


			BuildDrawItems(model, m_GlobalOverride, dist, queue.opaque, queue.transparent);
		}


		///shadow casters (separate List, unsorted)
		for (const Model* model : ctx.visibility.shadowCasters)
		{
			if (!model)
				return;

			BuildDrawItems(model, nullptr, 0.0f, queue.shadowCasters, queue.shadowCasters);
		}


		//sort
		SortFrontToBack(queue.opaque); //early-Z
		SortBackToFront(queue.transparent); // painter's algo

		//clear global override next frame
		m_GlobalOverride.reset();


		//store in context blackboard for passes to consume
		ctx.Set("BatchQueue", queue);
	}

	void RenderBatcher::BuildDrawItems(const Model* model, std::shared_ptr<Material> matOverride, float distanceToCamera, std::vector<DrawItem>& outOpaque, std::vector<DrawItem>& outTransparent)
	{

		//pre compute transform once per model
		DirectX::XMMATRIX worldMatrix = model->GetModelMatrix();
		DirectX::XMMATRIX normalMatrix = DirectX::XMMatrixTranspose(
			DirectX::XMMatrixInverse(nullptr, worldMatrix));


		//Resolve instancing data once per model
		const InstanceData* instanceData = model->IsInstanced() ? model->GetInstanceData() : nullptr;
		const SkinningData* skinData = model->IsSkinned() ? model->GetSkinningData() : nullptr;


		for (size_t mi = 0; mi < model->GetMeshCount(); ++mi)
		{
			std::shared_ptr<Mesh> mesh = model->GetMesh(mi);
			if (!mesh || !mesh->IsValid())
				continue;


			size_t submeshCount = std::max(size_t(1), mesh->GetSubmeshCount());

			for (size_t si = 0; si < submeshCount; ++si)
			{
				//resolve material: override > mesh material > skip
				std::shared_ptr<Material> mat = matOverride ? matOverride : model->GetMaterial(mi, si);

				if (!mat)
					continue;

				DrawItem item;
				DrawItem item;
				item.mesh = mesh;
				item.meshIndex = mi;
				item.submeshIndex = si;
				item.material = mat;
				item.distanceToCamera = distanceToCamera;
				item.stateKey = BuildStateKey(mat.get(), mesh.get());

				// Store transform as XMFLOAT4X4 for later upload
				DirectX::XMStoreFloat4x4(&item.modelMatrix, worldMatrix);
				DirectX::XMStoreFloat4x4(&item.normalMatrix, normalMatrix);

				// Instancing
				if (instanceData && instanceData->GetInstanceCount() > 0)
				{
					item.instanceTransforms = &instanceData->transforms;
					item.instanceCount = static_cast<uint32_t>(instanceData->GetInstanceCount());
				}

				// Skinning
				if (skinData && !skinData->boneMatrices.empty())
					item.boneMatrices = &skinData->boneMatrices;

				// Shadow flags from model
				item.castsShadow = model->CastsShadows();
				item.receivesShadows = model->ReceivesShadows();

				// Route to correct queue
				if (IsTransparent(mat.get()))
					outTransparent.push_back(item);
				else
					outOpaque.push_back(item);

			}
		}
	
	}

	void RenderBatcher::SortFrontToBack(std::vector<DrawItem>& items) const
	{
		// Primary: front-to-back (early Z rejection)
	// Secondary: by state key (minimise GPU state changes within same depth)
		std::sort(items.begin(), items.end(),
			[](const DrawItem& a, const DrawItem& b)
			{
				if (a.distanceToCamera != b.distanceToCamera)
					return a.distanceToCamera < b.distanceToCamera;
				return a.stateKey < b.stateKey;
			});
	}

	void RenderBatcher::SortBackToFront(std::vector<DrawItem>&items) const
	{
		std::sort(items.begin(), items.end(),
			[](const DrawItem& a, const DrawItem& b)
			{
				return a.distanceToCamera > b.distanceToCamera;
			});
	}

	uint64_t RenderBatcher::BuildStateKey(const Material * mat, const Mesh * mesh) const
	{
		// Pack two 32-bit halves of the pointers together.
		   // This gives consistent ordering that minimises material/pipeline changes.
		uintptr_t matPart = reinterpret_cast<uintptr_t>(mat) & 0xFFFFFFFF;
		uintptr_t meshPart = reinterpret_cast<uintptr_t>(mesh) & 0xFFFFFFFF;
		return (static_cast<uint64_t>(matPart) << 32) | meshPart;
	}

	bool RenderBatcher::IsTransparent(const Material* mat) const
	{
		if (!mat) return false;
		return mat->GetType() == MaterialType::Transparent;
	}


	
}
