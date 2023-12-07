#include "SkySphere.h"

SkySphere::SkySphere(Graphics& g)
	:
	sphere(4,1)
{
	skyPos = XMMatrixIdentity();
}

void SkySphere::Draw(Graphics& g, XMVECTOR camPos, XMVECTOR camTarget)
{
	//Define sphereWorld's world space matrix
	Scale = XMMatrixScaling(5.0f, 5.0f, 5.0f);
	//Make sure the sphere is always centered around camera
	Translation = XMMatrixTranslation(XMVectorGetX(camPos), XMVectorGetY(camPos), XMVectorGetZ(camPos));

	//Set sphereWorld's world space using the transformations
	skyPos = Scale * Translation;

	getSkyTexture(g).Bind(g);
	
	//getSky(g).DrawSky(g, skyPos, camPos);
	getSky(g).Draw(g, skyPos, camPos,camTarget);

}

Mesh SkySphere::getSky(Graphics& g)
{
	return Mesh(g, sphere.getIndices(), sphere.getVertex(),
		L"assets/shaders/skyVs.cso",
		L"assets/shaders/skyPs.cso"
	);
}

CubeMapTexture SkySphere::getSkyTexture(Graphics& g)
{
	const char* skyFilename[6] = {
		"assets/textures/SpaceBox/0.png",
		"assets/textures/SpaceBox/1.png",
		"assets/textures/SpaceBox/2.png",
		"assets/textures/SpaceBox/3.png",
		"assets/textures/SpaceBox/4.png",
		"assets/textures/SpaceBox/5.png"
	};
	return CubeMapTexture(g,skyFilename,1);
}
