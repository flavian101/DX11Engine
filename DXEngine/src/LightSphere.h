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
		LightSphere();
		void BindLight();
	private:
		Sphere m_Sphere;
		std::shared_ptr <DirectionalLight> m_Light;

	};

}