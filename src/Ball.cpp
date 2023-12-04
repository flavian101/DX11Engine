#include "Ball.h"

Ball::Ball(Graphics& g)
	:
	sphere(32),
	tex(g, "assets/textures/8k_moon.jpg")
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
	return Mesh(g,sphere.getIndices(),sphere.getVertex());
}
