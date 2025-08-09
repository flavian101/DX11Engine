#include "dxpch.h"
#include "Ball.h"
#include <utils/material/Material.h>

namespace DXEngine {

	Ball::Ball()
		:
		Model(),
		sphere(64)
	{
		Initialize();

		auto moonMaterial = DXEngine::MaterialFactory::CreateTexturedMaterial("MoonMaterial");
		auto moonTexture = std::make_shared<Texture>("assets/textures/8k_moon.jpg");
		moonMaterial->SetDiffuseTexture(moonTexture);
		moonMaterial->SetSpecularColor({ 0.2f, 0.2f, 0.2f, 1.0f });
		moonMaterial->SetShininess(100.0f);
		

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

