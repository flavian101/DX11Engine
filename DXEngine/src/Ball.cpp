#include "dxpch.h"
#include "Ball.h"

namespace DXEngine {

	Ball::Ball(const std::shared_ptr<ShaderProgram>& program)
		:
		Model(program),
		sphere(64)
	{
		Initialize();

		auto moonMaterial = std::make_shared<Material>();
		auto moonTexture = std::make_shared<Texture>("assets/textures/8k_moon.jpg");
		moonMaterial->SetDiffuse(moonTexture);
		moonMaterial->SetShaderProgram(program);
		m_Mesh->SetMaterial(moonMaterial);

	}
	void  Ball::Initialize()
	{
		if (!m_Mesh)
		{
			m_Mesh = std::make_shared<Mesh>(sphere.getMeshResource());
		}
	}
}

