#include "Ball.h"

Ball::Ball(Graphics& g)
	:
	sphere(64),
	tex(g, "assets/textures/8k_moon.jpg",0)
{

	ballPos = XMMatrixIdentity();
	Initialize(g);

}
void  Ball::Initialize(Graphics& g)
{
if (!ballMesh)
	{
		ballMesh = std::make_unique<Mesh>(g, sphere.getIndices(), sphere.getVertex(),
			L"assets/shaders/vs.cso",
			L"assets/shaders/ps.cso");
}
}

void Ball::Draw(Graphics& g, XMVECTOR camPos, XMVECTOR camTarget)
{
	Scale = XMMatrixScaling(10.0f, 10.0f, 10.0f);
	//Translation = XMMatrixTranslation(0.0f, 10.0f, 0.0f);

	ballPos = Scale * Translation;

	tex.Bind(g);
	ballMesh->Draw(g, ballPos, camPos, camTarget);

}

void Ball::SetPos(XMMATRIX trans)
{
	Translation = trans;
	
}

