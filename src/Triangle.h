#pragma once
#include "renderer/RendererCommand.h"
#include "utils\Mesh.h"
#include <DirectXMath.h>
#include "utils\Texture.h"
#include "models\Model.h"

namespace DXEngine {

	class Triangle : public Model
	{
	public:
		Triangle( std::shared_ptr<ShaderProgram> program);

		void Render()override;
		void Initialize();
	private:

	};
}
