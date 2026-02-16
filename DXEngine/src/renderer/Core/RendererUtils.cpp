#include "RendererUtils.h"
#include "models/Model.h"
#include "utils/Mesh/Mesh.h"
#include "renderer/MaterialSystem.h"
#include "renderer/ShaderCache.h"
#include "utils/UI/UIElement.h"
#include "utils/Light.h"
#include <DirectXCollision.h>
#include <algorithm>
namespace DXEngine
{
	bool RenderSubmission::IsValid() const
	{
		if (isUIElement)
		{
			return uiElement != nullptr && material != nullptr;
		}
		else
		{
			return mesh != nullptr && mesh->IsValid() && material != nullptr && srcModel != nullptr;
		}
	}
	RenderSubmission RenderSubmission::CreateFromModel(const Model* model, size_t meshIndex, size_t submeshIndex)
	{
		RenderSubmission submission;
		if (!model || !model->IsValid())
			return submission;

		submission.srcModel = model;
		submission.meshIndex = meshIndex;
		submission.submeshIndex = submeshIndex;
		submission.mesh = model->GetMesh(meshIndex);

		if (submission.mesh)
		{
			submission.material = submission.mesh->GetMaterial(submeshIndex);
		}

		//store transform- Model::GetModelMatrix return XMMAtRIX
		DirectX::XMMATRIX modelMatrix = model->GetModelMatrix();
		DirectX::XMStoreFloat4x4(&submission.modelMatrix, modelMatrix);

		//calculate normal matrix
		DirectX::XMMATRIX normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, modelMatrix));
		DirectX::XMStoreFloat4x4(&submission.normalMatrix, normalMatrix);

		// Visibility and shadow flags
		submission.visible = model->IsVisible();
		submission.castsShadow = model->CastsShadows();
		submission.receivesShadows = model->ReceivesShadows();

		// Instancing support - check if model is instanced
		if (model->IsInstanced())
		{
			const InstanceData* instanceData = model->GetInstanceData();
			if (instanceData && instanceData->GetInstanceCount() > 0)
			{
				submission.instanceTransforms = &instanceData->transforms;
				submission.instanceCount = instanceData->GetInstanceCount();
			}
		}

		// Skinning support - check if model is skinned
		if (model->IsSkinned())
		{
			const SkinningData* skinData = model->GetSkinningData();
			if (skinData && !skinData->boneMatrices.empty())
			{
				submission.boneMatrices = &skinData->boneMatrices;
			}
		}

		return submission;
	}
	RenderSubmission RenderSubmission::CreateFromUIElement(std::shared_ptr<UIElement> element, std::shared_ptr<Material> material)
	{
		RenderSubmission submission;
		submission.uiElement = element;
		submission.material = material;
		submission.queue = RenderQueue::UI;
		submission.isUIElement = true;
		submission.visible = element ? element->IsVisible() : false;

		DirectX::XMStoreFloat4x4(&submission.modelMatrix, DirectX::XMMatrixIdentity());
		DirectX::XMStoreFloat4x4(&submission.normalMatrix, DirectX::XMMatrixIdentity());

		return submission;
	}

	//RenderStatistics
	std::string RenderStatistics::ToString() const
	{
		std::ostringstream oss;
		oss << "=== Render Statistics (Frame " << frameNumber << ") ===\n";
		oss << "Time: " << totalTime << "s (Delta: " << deltaTime << "s)\n\n";

		oss << "Submissions:\n";
		oss << "  Models: " << modelsSubmitted << "\n";
		oss << "  Meshes: " << meshesRendered << "\n";
		oss << "  Submeshes: " << submeshesRendered << "\n";
		oss << "  UI Elements: " << uiElementsRendered << "\n";
		oss << "  Total Processed: " << submissionsProcessed << "\n\n";

		oss << "Draw Calls:\n";
		oss << "  Regular: " << drawCalls << "\n";
		oss << "  Instanced: " << instanceDrawCalls << "\n";
		oss << "  Instances: " << instancesRendered << "\n";
		oss << "  Batches: " << batchesProcessed << "\n\n";

		oss << "Geometry:\n";
		oss << "  Triangles: " << trianglesRendered << "\n";
		oss << "  Vertices: " << verticesRendered << "\n\n";

		oss << "State Changes:\n";
		oss << "  Pipelines: " << pipelineChanges << "\n";
		oss << "  Materials: " << materialsChanged << "\n";
		oss << "  Shaders: " << shadersChanged << "\n\n";

		oss << "Culling:\n";
		oss << "  Objects Culled: " << objectsCulled << "\n";
		oss << "  Lights Active: " << lightsProcessed << "\n\n";

		oss << "Memory:\n";
		oss << "  GPU: " << (gpuMemoryUsed / 1024 / 1024) << " MB\n";
		oss << "  CPU: " << (cpuMemoryUsed / 1024 / 1024) << " MB\n";

		return oss.str();
	}
}