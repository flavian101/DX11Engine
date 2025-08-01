#include "SkySphere.h"

SkySphere::SkySphere(Graphics& gfx, std::shared_ptr<ShaderProgram> program)
	:Model(gfx, program)
{
	sphere = std::make_unique<Sphere>("flavian", 4);
	Initialize(gfx);

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
	auto skyTexture = std::make_shared<CubeMapTexture>(gfx, skyFilename);
	auto skyMaterial = std::make_shared<Material>(gfx);
	skyMaterial->SetShaderProgram(program);
	skyMaterial->SetSkyMaterial(skyTexture);
	m_Mesh->SetMaterial(skyMaterial);
}

void SkySphere::Initialize(Graphics& gfx) {
	//if (!initialized) {
		// Create mesh once
	m_Mesh = std::make_shared<Mesh>(gfx,sphere->getMeshResource());
		initialized = true;
	//}
}

void SkySphere::Render(Graphics& gfx)
{
	// Use stored resources instead of creating new ones
	XMFLOAT3 camPos = { XMVectorGetX(gfx.GetCamera().GetPos()),
						XMVectorGetY(gfx.GetCamera().GetPos()) ,
						XMVectorGetZ(gfx.GetCamera().GetPos()) };

	SetTranslation(camPos);
	gfx.SetRasterizerMode(RasterizerMode::SolidFrontCull);
	gfx.SetDepthLessEqual();
	Model::Render(gfx);
	

}