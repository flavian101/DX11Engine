#pragma once
#include "renderer/RendererCommand.h"
#include "utils/Texture.h"
#include <utils/mesh/Mesh.h>
#include "models/Model.h"


namespace DXEngine {

	class Ball : public Model
	{
	public:
		Ball();
	private:
		void Initialize();



	};

}