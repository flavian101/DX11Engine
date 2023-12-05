#include "Ball.h"

Ball::Ball(Graphics& g)
	:
	sphere(4),
	tex(g, "assets/textures/icon.png",0)
{

	ballPos = XMMatrixIdentity();

}

void Ball::Draw(Graphics& g, XMVECTOR camPos, XMVECTOR camTarget)
{
	Scale = XMMatrixScaling(10.0f, 10.0f, 10.0f);
	Translation = XMMatrixTranslation(0.0f, 10.0f, 0.0f);

	ballPos = Scale * Translation;

	tex.Bind(g);
	getBal(g).Draw(g, ballPos, camPos, camTarget);

}

Mesh Ball::getBal(Graphics& g)
{
	return Mesh(g,sphere.getIndices(),sphere.getVertex(),
		L"assets/shaders/vs.hlsl",
		L"assets/shaders/ps.hlsl"
	);
}
