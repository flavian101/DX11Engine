#include "Ball.h"

Ball::Ball(Graphics& gfx, std::shared_ptr<ShaderProgram> program)
	:
	Model(gfx,program),
	sphere(64)
{
	Initialize(gfx);

	auto moonMaterial = std::make_shared<Material>(gfx);
	auto moonTexture = std::make_shared<Texture>(gfx, "assets/textures/8k_moon.jpg");
	moonMaterial->SetDiffuse(moonTexture);
	moonMaterial->SetShaderProgram(program);
	m_Mesh->SetMaterial(moonMaterial);

}
void  Ball::Initialize(Graphics& g)
{
	if (!m_Mesh)
	{
		m_Mesh = std::make_shared<Mesh>(g,sphere.getMeshResource());
	}
}

void Ball::Render(Graphics& gfx)
{
	Model::Render(gfx);
}


