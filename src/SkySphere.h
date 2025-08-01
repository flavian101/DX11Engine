#pragma once
#include "utils\Mesh.h"
#include "Sphere.h"
#include <wrl.h>
#include <memory>
#include <models/Model.h>



class SkySphere : public Model
{
	
public:
	SkySphere(Graphics& gfx, std::shared_ptr<ShaderProgram> program);

	void Render(Graphics& gfx) override;
	void Initialize(Graphics& gfx);


private:
	bool initialized = false;
	std::unique_ptr<Sphere> sphere;


};

