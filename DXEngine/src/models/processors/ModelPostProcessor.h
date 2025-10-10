#pragma once
#include "ModelLoaderUtils.h"

namespace DXEngine
{
	class Model;

	class ModelPostProcessor
	{
	public:
		ModelPostProcessor() = default;

		void PostProcess(std::shared_ptr<Model> model, const ModelLoadOptions& options);

		// Individual operations
		void OptimizeModel(std::shared_ptr<Model> model);
		void ValidateModel(std::shared_ptr<Model> model);
		void EnsureDefaultMaterials(std::shared_ptr<Model> model);
		void ApplyGlobalScale(std::shared_ptr<Model> model, float scale);
	private:
		std::string m_LastError;
		
	};
}
