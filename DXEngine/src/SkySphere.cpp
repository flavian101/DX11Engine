#include "dxpch.h"
#include "SkySphere.h"
#include <utils/material/Material.h>
#include "utils/CubeMapTexture.h"

namespace DXEngine {

	SkySphere::SkySphere()
		:Model()
	{
		sphere = std::make_unique<Sphere>("flavian", 4);
		Initialize();

		auto skyMaterial = DXEngine::MaterialFactory::CreateSkyboxMaterial("SkyMaterial");

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
		auto skyTexture = std::make_shared<CubeMapTexture>(skyFilename);
		skyMaterial->SetEnvironmentTexture(skyTexture);
		skyMaterial->SetEmissiveColor({ 0.1f,0.01f,0.01f,0.4f });
		m_Mesh->SetMaterial(skyMaterial);
	}

	void SkySphere::Initialize() {
		//if (!initialized) {
			// Create mesh once
		m_Mesh = std::make_shared<Mesh>(sphere->getMeshResource());
		initialized = true;
		//}
	}
}