#pragma once
#include <memory>
#include "utils/material/Material.h"
#include <map>
#include <vector>



namespace DXEngine
{
	class Model;
	class Mesh;
	class MaterialSystem;
	class ShaderCache;
	class UIElement;
	class LightManager;

	struct RenderSubmission
	{
		//source object
		const Model* srcModel = nullptr;
		std::shared_ptr<Mesh> mesh;
		size_t meshIndex = 0;
		size_t submeshIndex = 0;

		//material
		std::shared_ptr<Material> material;
		std::shared_ptr<Material> materialOverride;

		//Transform
		DirectX::XMFLOAT4X4 modelMatrix;
		DirectX::XMFLOAT4X4 normalMatrix;

		//Render State
		RenderQueue queue = RenderQueue::Opaque;
		float sortKey = 0.0f;
		uint64_t batchKey = 0; //for sorting and batching

		//Visibility
		bool visible = true;
		bool castsShadow = true;
		bool receivesShadows = true;

		//Instancing 
		const std::vector<DirectX::XMFLOAT4X4>* instanceTransforms = nullptr;
		size_t instanceCount = 0;

		//skinning
		const std::vector<DirectX::XMFLOAT4X4>* boneMatrices = nullptr;

		// UI elements
		std::shared_ptr<UIElement> uiElement;
		bool isUIElement = false;

		//Helper Methods
		std::shared_ptr<Material> GetEffectiveMaterial()const
		{
			return materialOverride ? materialOverride : material;
		}

		bool IsValid() const;
		bool IsInstanced() const { return instanceTransforms && instanceCount > 0; }
		bool IsSkinned() const { return boneMatrices && !boneMatrices->empty(); }

		// Factory methods
		static RenderSubmission CreateFromModel(const Model* model, size_t meshIndex = 0, size_t submeshIndex = 0);
		static RenderSubmission CreateFromUIElement(std::shared_ptr<UIElement> element, std::shared_ptr<Material> material);
	};

	//Render batch
	struct RenderBatch
	{
		RenderQueue queue = RenderQueue::Opaque;
		std::vector<RenderSubmission> submissions;
		std::shared_ptr<Material> batchMaterial;
		std::shared_ptr<Mesh> batchMesh;
		bool isInstanced = false;
		bool requiresDepthSorting = false;

		void Clear() { submissions.clear(); }
		bool IsEmpty() const { return submissions.empty(); }
		size_t Size() const { return submissions.size(); }
	};

	//Render Statics
	struct RenderStatistics
	{
		// Frame info
		uint32_t frameNumber = 0;
		float deltaTime = 0.0f;
		float totalTime = 0.0f;

		// Submission stats
		uint32_t modelsSubmitted = 0;
		uint32_t meshesRendered = 0;
		uint32_t submeshesRendered = 0;
		uint32_t uiElementsRendered = 0;

		// Draw stats
		uint32_t drawCalls = 0;
		uint32_t instanceDrawCalls = 0;
		uint32_t instancesRendered = 0;
		uint32_t trianglesRendered = 0;
		uint32_t verticesRendered = 0;

		// Batching stats
		uint32_t batchesProcessed = 0;
		uint32_t submissionsProcessed = 0;

		// State changes
		uint32_t pipelineChanges = 0;
		uint32_t materialsChanged = 0;
		uint32_t shadersChanged = 0;

		// Culling stats
		uint32_t objectsCulled = 0;
		uint32_t lightsProcessed = 0;

		// Memory stats
		size_t gpuMemoryUsed = 0;
		size_t cpuMemoryUsed = 0;

		void Reset()
		{
			// Keep frame info, reset counters
			modelsSubmitted = 0;
			meshesRendered = 0;
			submeshesRendered = 0;
			uiElementsRendered = 0;
			drawCalls = 0;
			instanceDrawCalls = 0;
			instancesRendered = 0;
			trianglesRendered = 0;
			verticesRendered = 0;
			batchesProcessed = 0;
			submissionsProcessed = 0;
			pipelineChanges = 0;
			materialsChanged = 0;
			shadersChanged = 0;
			objectsCulled = 0;
			lightsProcessed = 0;
		}

		std::string ToString() const;
	};

	//Renderer Configuration

	struct RendererConfig
	{
		// Rendering features
		bool enableInstancing = true;
		bool enableFrustumCulling = true;
		bool enableOcclusionCulling = false;
		bool enableShadows = true;
		bool enableWireframe = false;

		// Quality settings
		uint32_t maxInstanceBatchSize = 100;
		uint32_t maxDrawCallsPerFrame = 10000;
		uint32_t shadowMapResolution = 2048;

		// Debug
		bool enableDebugInfo = false;
		bool enableGPUValidation = false;
		bool showBoundingBoxes = false;
	};

	struct PipelineKey
	{
		size_t vertexLayoutHash;
		RHI::CullMode cullMode;
		RHI::DepthTestMode depthTest;
		RHI::BlendMode blendMode;
		RHI::PrimitiveTopology topology;
		bool wireframe;

		bool operator==(const PipelineKey& other) const
		{
			return vertexLayoutHash == other.vertexLayoutHash &&
				cullMode == other.cullMode &&
				depthTest == other.depthTest &&
				blendMode == other.blendMode &&
				topology == other.topology &&
				wireframe == other.wireframe;
		}
	};

	struct PipelineKeyHash
	{
		size_t operator()(const PipelineKey& key)const
		{
			size_t h1 = std::hash<size_t>()(key.vertexLayoutHash);
			size_t h2 = std::hash<int>()(static_cast<int>(key.cullMode));
			size_t h3 = std::hash<int>()(static_cast<int>(key.depthTest));
			size_t h4 = std::hash<int>()(static_cast<int>(key.blendMode));
			size_t h5 = std::hash<int>()(static_cast<int>(key.topology));
			size_t h6 = std::hash<bool>()(key.wireframe);

			return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5);
		}
	};
}
