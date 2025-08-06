#pragma once
#include "utils\Mesh.h"
#include "Sphere.h"
#include <wrl.h>
#include <memory>
#include <models/Model.h>


namespace DXEngine {

	class SkySphere : public Model
	{

	public:
		SkySphere( std::shared_ptr<ShaderProgram> program);

		void Render() override;
		void Initialize();


	private:
		bool initialized = false;
		std::unique_ptr<Sphere> sphere;


	};

}