#pragma once
#include "Graphics.h"
#include "utils\Mesh.h"
#include <DirectXMath.h>
#include "utils\Texture.h"
#include "models\Model.h"

namespace DXEngine {

	class Triangle : public Model
	{
	public:
		Triangle(Graphics& gfx, std::shared_ptr<ShaderProgram> program);

		void Render(Graphics& gfx)override;
		void Initialize(Graphics& gfx);
	private:

	};
}
