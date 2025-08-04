#pragma once
#include "utils\Light.h"
#include "Sphere.h"
#include "utils\Mesh.h"
#include "models/Model.h"
namespace DXEngine {

	class LightSphere : public Model
	{
	public:
		LightSphere(Graphics& gfx, std::shared_ptr<ShaderProgram> program);
		void Render(Graphics& gfx) override;
	private:
		Sphere m_Sphere;
		std::shared_ptr <PointLight> m_Light;

	};

}