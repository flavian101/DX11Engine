#include "EditorLayer.h"
#include "utils/ImageLoader.h"
#include "utils/material/Material.h"

using namespace DXEngine;

EditorLayer::EditorLayer()
	: Layer("EditorLayer")
{
}

EditorLayer::~EditorLayer() = default;

void EditorLayer::OnAttach()
{
	Renderer::SetEnvironment(RenderEnvironment());      // defaults: dim bluish ambient, exposure 0.4, gamma 2.2, clustered lights

	m_CameraController = std::make_shared<CameraController>();
	Camera& camera = *m_CameraController->GetCamera();
	camera.SetPosition({ 0.0f, 6.0f, 20.0f });
	camera.SetRotation({ -0.18f, 0.0f, 0.0f });          // looks a little down (negative pitch), toward -Z

	BuildDefaultScene();

	m_Editor.SetScene(&m_Scene);
	m_Editor.SetCamera(m_CameraController);
}

void EditorLayer::BuildDefaultScene()
{
	auto device = Renderer::GetDevice();

	// ---- sky
	SceneFactory::CreateSky(m_Scene, device, {
		"assets/textures/NightSky/nightBack.png",   "assets/textures/NightSky/nightBottom.png",
		"assets/textures/NightSky/nightFront.png",  "assets/textures/NightSky/nightLeft.png",
		"assets/textures/NightSky/nightRight.png",  "assets/textures/NightSky/nightTop.png" });

	// ---- ground: a big plane with a tiled grass material.
	// (The small grass pair on purpose: the 'grass_with_mud_and_stones' set next to it is 80 MB of JPEG, which is most of what
	//  is left of the start-up time once no model is loaded. Swap the two paths for it when the asset system streams textures.)
	{
		auto grass = MaterialFactory::CreateLitMaterial("Ground Material");
		grass->SetTexture(TextureSlot::Diffuse, ImageLoader::LoadForSlot(*device, "assets/textures/grass.jpg",       TextureSlot::Diffuse));
		grass->SetTexture(TextureSlot::Normal,  ImageLoader::LoadForSlot(*device, "assets/textures/grassNormal.jpg", TextureSlot::Normal));
		grass->SetTextureScale({ 80.0f, 80.0f });
		Entity ground = SceneFactory::CreatePlane(m_Scene, device, "Ground", 1.0f, 1.0f, grass);
		ground.Get<TransformComponent>().scale = { 500.0f, 1.0f, 500.0f };
		ground.Get<MeshRendererComponent>().castsShadows = false;   // a 500 m plane in every shadow view would only cost; it RECEIVES shadows
	}

	// ---- moon: a textured sphere far away
	{
		auto moon = MaterialFactory::CreateLitMaterial("Moon Material");
		moon->SetTexture(TextureSlot::Diffuse, ImageLoader::LoadForSlot(*device, "assets/textures/8k_moon.jpg", TextureSlot::Diffuse));
		moon->SetSpecularColor({ 0.2f, 0.2f, 0.2f, 1.0f });
		moon->SetShininess(100.0f);
		Entity moonEntity = SceneFactory::CreateSphere(m_Scene, device, "Moon", 1.0f, 64, moon);
		moonEntity.Get<TransformComponent>().position = { 100.0f, 40.0f, -60.0f };
		moonEntity.Get<TransformComponent>().scale = { 20.0f, 20.0f, 20.0f };
	}

	// ---- a row of spheres: metallic goes 0 -> 1 from left to right, roughness 1 -> 0.15: something for the lights to show off
	{
		const int count = 6;
		for (int i = 0; i < count; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(count - 1);
			auto mat = MaterialFactory::CreateLitMaterial("Sphere " + std::to_string(i + 1));
			mat->SetDiffuseColor({ 0.85f, 0.55f, 0.25f, 1.0f });
			mat->SetMetallic(t);
			mat->SetRoughness(1.0f - 0.85f * t);
			Entity ball = SceneFactory::CreateSphere(m_Scene, device, "Sphere " + std::to_string(i + 1), 1.4f, 48, mat);
			ball.Get<TransformComponent>().position = { -7.5f + 3.0f * static_cast<float>(i), 1.4f, 0.0f };
		}

		auto stone = MaterialFactory::CreateLitMaterial("Cube Material");
		stone->SetDiffuseColor({ 0.55f, 0.58f, 0.62f, 1.0f });
		stone->SetRoughness(0.7f);
		Entity cube = SceneFactory::CreateCube(m_Scene, device, "Cube", 3.0f, stone);
		cube.Get<TransformComponent>().position = { 0.0f, 1.5f, -8.0f };
		cube.Get<TransformComponent>().SetEulerDegrees(0.0f, 25.0f, 0.0f);
	}

	// ---- a lamp: a point light entity with a small glowing sphere as its child, so the marker always sits exactly where the light is
	{
		Entity lamp = SceneFactory::CreatePointLight(m_Scene, "Lamp", { 10.0f, 15.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 300.0f, 40.0f);
		lamp.Get<LightComponent>().castsShadows = true;
		auto glow = MaterialFactory::CreateEmissiveMaterial("Lamp Glow");
		glow->SetEmissiveColor({ 1.0f, 0.5f, 1.0f, 1.0f });
		glow->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		Entity marker = SceneFactory::CreateSphere(m_Scene, device, "Lamp Marker", 1.5f, 16, glow);
		m_Scene.SetParent(marker, lamp);
		marker.Get<TransformComponent>().scale = { 2.0f, 2.0f, 2.0f };
		marker.Get<MeshRendererComponent>().castsShadows = false;
	}

	// ---- shadows: a dim moonlight with cascaded shadows, and a warm spot light on the spheres
	{
		Entity moonlight = SceneFactory::CreateDirectionalLight(m_Scene, "Moonlight", { -0.4f, -0.8f, 0.45f }, { 0.55f, 0.65f, 1.0f }, 1.2f);
		moonlight.Get<LightComponent>().castsShadows = true;

		Entity spot = m_Scene.CreateEntity("Spot Light");
		auto& t = spot.Get<TransformComponent>();
		t.position = { -18.0f, 22.0f, 12.0f };
		t.SetForward({ 18.0f, -20.0f, -12.0f });                     // toward the spheres
		auto& l = spot.Add<LightComponent>();
		l.type = LightType::Spot;
		l.color = { 1.0f, 0.9f, 0.7f };
		l.intensity = 400.0f;
		l.range = 50.0f;
		l.innerConeDegrees = 18.0f;
		l.outerConeDegrees = 32.0f;
		l.castsShadows = true;
	}
}

void EditorLayer::OnUpdate(float dt)
{
	Renderer::SetClearColor(0.1f, 0.1f, 0.16f);

	// F1: edit <-> play. (Polled, so not while a text field is being edited.)
	const bool f1Down = Input::IsKeyPressed(VK_F1);
	if (f1Down && !m_F1WasDown && !Application::Get().GetImGuiLayer().WantsTextInput())
		m_Editor.SetEnabled(!m_Editor.IsEnabled());
	m_F1WasDown = f1Down;

	m_CameraController->Update(dt);                   // (the editor decides when the camera listens: Editor::SetCamera)
	AnimationSystem::Update(m_Scene, dt);             // advance animations (before transforms are derived)

	Renderer::BeginScene(m_CameraController->GetCamera());
	Renderer::Submit(m_Scene);                        // the renderer reads the persistent scene
	Renderer::EndScene();
}

void EditorLayer::OnUIRender()
{
	m_Editor.OnImGui();
}

void EditorLayer::OnEvent(DXEngine::Event& event)
{
	if (!m_Editor.OnEvent(event))                     // the mouse wheel sets the fly speed while flying ...
		m_CameraController->OnEvent(event);           // ... everything else is the camera's
}
