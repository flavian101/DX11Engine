#pragma once
#include "utils\Light.h"
#include "Sphere.h"
#include "utils\Mesh.h"
#include "models/Model.h"
#include <memory>

namespace DXEngine {

	class LightSphere : public Model
	{
	public:
		LightSphere(const std::shared_ptr<ShaderProgram>& program);
		void Render() override;
	private:
		Sphere m_Sphere;
		std::shared_ptr <PointLight> m_Light;

	};

}