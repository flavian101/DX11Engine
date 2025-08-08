#pragma once
#include "utils\Mesh.h"
#include "Sphere.h"
#include "dxpch.h"
#include <memory>
#include <models/Model.h>


namespace DXEngine {

	class SkySphere : public Model
	{

	public:
		SkySphere();

		void Initialize();


	private:
		bool initialized = false;
		std::unique_ptr<Sphere> sphere;


	};

}