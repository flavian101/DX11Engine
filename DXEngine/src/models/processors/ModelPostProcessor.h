#pragma once
#include "ModelLoaderUtils.h"
#include <memory>

namespace DXEngine
{
	class ModelAsset;

	// The steps that run on an imported asset after the importer built it. Whatever goes wrong is written to
	// ModelAsset::warnings (the editor shows them): nothing is dropped silently.
	class ModelPostProcessor
	{
	public:
		ModelPostProcessor() = default;

		void PostProcess(const std::shared_ptr<ModelAsset>& asset, const ModelLoadOptions& options);

		// Individual operations
		void OptimizeModel(const std::shared_ptr<ModelAsset>& asset);        // meshoptimizer: see MeshResource::OptimizeForRendering
		void ValidateModel(const std::shared_ptr<ModelAsset>& asset);        // structure problems -> warnings
		void EnsureDefaultMaterials(const std::shared_ptr<ModelAsset>& asset);
		void ApplyGlobalScale(const std::shared_ptr<ModelAsset>& asset, float scale);
	};
}
