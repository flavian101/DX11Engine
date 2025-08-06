#include "SkySphere.h"
namespace DXEngine {

	SkySphere::SkySphere( std::shared_ptr<ShaderProgram> program)
		:Model(, program)
	{
		sphere = std::make_unique<Sphere>("flavian", 4);
		Initialize();

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
		auto skyTexture = std::make_shared<CubeMapTexture>(, skyFilename);
		auto skyMaterial = std::make_shared<Material>();
		skyMaterial->SetShaderProgram(program);
		skyMaterial->SetSkyMaterial(skyTexture);
		m_Mesh->SetMaterial(skyMaterial);
	}

	void SkySphere::Initialize() {
		//if (!initialized) {
			// Create mesh once
		m_Mesh = std::make_shared<Mesh>(, sphere->getMeshResource());
		initialized = true;
		//}
	}

	void SkySphere::Render()
	{
		// Use stored resources instead of creating new ones
		DirectX::XMFLOAT3 camPos = { DirectX::XMVectorGetX(RenderCommand:: GetCamera().GetPos()),
							DirectX::XMVectorGetY(RenderCommand:: GetCamera().GetPos())
							DirectX::XMVectorGetZ(RenderCommand:: GetCamera().GetPos()) };

		SetTranslation(camPos);
		RenderCommand:: SetRasterizerMode(RasterizerMode::SolidFrontCull);
		RenderCommand:: SetDepthLessEqual();
		Model::Render();


	}
}