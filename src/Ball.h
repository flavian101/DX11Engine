#pragma once
#include "Sphere.h"
#include "Graphics.h"
#include "utils/Texture.h"
#include <utils/Mesh.h>
#include "models/Model.h"


namespace DXEngine {

	class Ball : public Model
	{
	public:
		Ball(Graphics& g, std::shared_ptr<ShaderProgram> program);
		void Render(Graphics& gfx)override;
	private:
		void Initialize(Graphics& g);
	private:
		Sphere sphere;


	};

}