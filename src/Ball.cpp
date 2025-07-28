#include "Ball.h"

Ball::Ball(Graphics& g)
	:
	sphere(64)
{
	ballPos = XMMatrixIdentity();
	Initialize(g);

	auto moonMaterial = std::make_shared<Material>(g);
	auto moonTexture = std::make_shared<Texture>(g, "assets/textures/8k_moon.jpg");
	moonMaterial->SetDiffuse(moonTexture);
	ballMesh->SetMaterial(moonMaterial);

}
void  Ball::Initialize(Graphics& g)
{
if (!ballMesh)
	{
		ballMesh = std::make_unique<Mesh>(g, sphere.GetIndices(), sphere.GetVertices(),
			L"assets/shaders/vs.cso",
			L"assets/shaders/ps.cso");
}
}

void Ball::Draw(Graphics& g, XMVECTOR camPos, XMVECTOR camTarget)
{
	Scale = XMMatrixScaling(10.0f,10.0f, 10.0f);
	//Translation = XMMatrixTranslation(0.0f, 10.0f, 0.0f);

	ballPos = Scale * Translation;

	ballMesh->Draw(g, ballPos, camPos, camTarget);

}

void Ball::SetPos(XMMATRIX trans)
{
	Translation = trans;
	
}

