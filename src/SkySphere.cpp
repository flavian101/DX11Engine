#include "SkySphere.h"

SkySphere::SkySphere(Graphics& g)
	
	
{
	sphere = std::make_unique<Sphere>("flavian", 4);
	skyPos = XMMatrixIdentity();
}

void SkySphere::Draw(Graphics& g, FXMVECTOR camPos)
{
	//Define sphereWorld's world space matrix
	Scale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	//Make sure the sphere is always centered around camera
	Translation = XMMatrixTranslation(XMVectorGetX(camPos), XMVectorGetY(camPos), XMVectorGetZ(camPos));

	//Set sphereWorld's world space using the transformations
	skyPos = Scale * Translation;

	getSkyTexture(g).Bind(g);
	
	getSky(g).DrawSky(g, skyPos);
	//getSky(g).Draw(g, skyPos, camPos,camTarget);

}



Mesh SkySphere::getSky(Graphics& g)
{
	return Mesh(g, sphere.get()->getIndices(), sphere.get()->getVertex(),
		L"assets/shaders/skyVs.cso",
		L"assets/shaders/skyPs.cso"
	);
}

CubeMapTexture SkySphere::getSkyTexture(Graphics& g)
{
	const char* skyFilename[6] = {
		"assets/textures/NightSky/nightBack.png",
		"assets/textures/NightSky/nightBottom.png",
		"assets/textures/NightSky/nightFront.png",
		"assets/textures/NightSky/nightLeft.png",
		"assets/textures/NightSky/nightRight.png",
		"assets/textures/NightSky/nightTop.png"
	};
	//const char* skyFilename[6] = {
	//	"assets/textures/Skybox/back.png",
	//	"assets/textures/Skybox/bottom.png",
	//	"assets/textures/Skybox/front.png",
	//	"assets/textures/Skybox/left.png",
	//	"assets/textures/Skybox/right.png",
	//	"assets/textures/Skybox/top.png"
	//};
	return CubeMapTexture(g,skyFilename,1);
}
