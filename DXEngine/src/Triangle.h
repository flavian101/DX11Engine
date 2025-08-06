#pragma once
#include "renderer/RendererCommand.h"
#include "models/Model.h"

namespace DXEngine {

	class Triangle : public Model
	{
	public:
		Triangle(const std::shared_ptr<ShaderProgram>& program);

		void Render()override;
		void Initialize();
	private:

	};
}
