#include "SkySphere.h"

SkySphere::SkySphere(Graphics& g)
	
	
{
	sphere = std::make_unique<Sphere>("flavian", 4);
	Initialize(g);
}

void SkySphere::Initialize(Graphics& g) {
	//if (!initialized) {
		// Create mesh once
		skyMesh = std::make_unique<Mesh>(g,
			sphere->getIndices(),
			sphere->getVertex(),
			L"assets/shaders/skyVs.cso",
			L"assets/shaders/skyPs.cso");

		const char* skyFilename[6] = {
			"assets/textures/NightSky/nightBack.png",
			"assets/textures/NightSky/nightBottom.png",
			"assets/textures/NightSky/nightFront.png",
			"assets/textures/NightSky/nightLeft.png",
			"assets/textures/NightSky/nightRight.png",
			"assets/textures/NightSky/nightTop.png"
		};
		 //Create texture once
		//const char* skyFilename[6] = {
		//	"assets/textures/Skybox/back.png",
		//	"assets/textures/Skybox/bottom.png",
		//	"assets/textures/Skybox/front.png",
		//	"assets/textures/Skybox/left.png",
		//	"assets/textures/Skybox/right.png",
		//	"assets/textures/Skybox/top.png"
		//};

		//const char* skyFilename[6] = {
		//	"assets/textures/SpaceBox/0.png",
		//	"assets/textures/SpaceBox/1.png",
		//	"assets/textures/SpaceBox/2.png",
		//	"assets/textures/SpaceBox/3.png",
		//	"assets/textures/SpaceBox/4.png",
		//	"assets/textures/SpaceBox/5.png"
		//};
		skyTexture = std::make_unique<CubeMapTexture>(g, skyFilename, 1);
		initialized = true;
	//}
}

void SkySphere::Draw(Graphics& g, FXMVECTOR camPos)
{
	XMMATRIX skyPos = XMMatrixIdentity();
	DirectX::XMMATRIX Scale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	DirectX::XMMATRIX Translation = XMMatrixTranslation(
		XMVectorGetX(camPos),
		XMVectorGetY(camPos),
		XMVectorGetZ(camPos));

	skyPos = Scale * Translation;

	// Use stored resources instead of creating new ones
	skyTexture->Bind(g);
	skyMesh->DrawSky(g, skyPos);

}