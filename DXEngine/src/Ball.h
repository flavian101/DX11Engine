#pragma once
#include "Sphere.h"
#include "renderer/RendererCommand.h"
#include "utils/Texture.h"
#include <utils/Mesh.h>
#include "models/Model.h"


namespace DXEngine {

	class Ball : public Model
	{
	public:
		Ball(const std::shared_ptr<ShaderProgram>& program);
		void Render()override;
	private:
		void Initialize();
	private:
		Sphere sphere;


	};

}