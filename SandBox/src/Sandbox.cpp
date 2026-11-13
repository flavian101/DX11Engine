#include "Sandbox.h"
#include "models/Model.h"
#include "utils/ImageLoader.h"
#include "utils/material/Material.h"
#include <random>
#include <string>

using namespace DXEngine;

Sandbox::Sandbox()
	:Layer("sanbox")
{}

Sandbox::~Sandbox()
{
}

void Sandbox::OnAttach()
{
	auto device = Renderer::GetDevice();
	m_Environment = RenderEnvironment();                // defaults: dim bluish ambient, exposure 0.4, gamma 2.2, clustered lights
	Renderer::SetEnvironment(m_Environment);

	m_CameraController = std::make_shared<CameraController>();
	m_Loader = std::make_shared<ModelLoader>();

	// ---- sky
	SceneFactory::CreateSky(m_Scene, device, {
		"assets/textures/NightSky/nightBack.png",   "assets/textures/NightSky/nightBottom.png",
		"assets/textures/NightSky/nightFront.png",  "assets/textures/NightSky/nightLeft.png",
		"assets/textures/NightSky/nightRight.png",  "assets/textures/NightSky/nightTop.png" });

	// ---- ground: a big plane with a tiled grass material
	{
		auto grass = MaterialFactory::CreateLitMaterial("Ground Material");
		const std::string dir = "assets/textures/grass/grass_with_mud_and_stones_39_46_";
		grass->SetTexture(TextureSlot::Diffuse,          ImageLoader::LoadForSlot(*device, dir + "diffuse.jpg", TextureSlot::Diffuse));
		grass->SetTexture(TextureSlot::Normal,           ImageLoader::LoadForSlot(*device, dir + "normal.jpg", TextureSlot::Normal));
		grass->SetTexture(TextureSlot::Roughness,        ImageLoader::LoadForSlot(*device, dir + "roughness.jpg", TextureSlot::Roughness));
		grass->SetTexture(TextureSlot::AmbientOcclusion, ImageLoader::LoadForSlot(*device, dir + "ao.jpg", TextureSlot::AmbientOcclusion));
		grass->SetTextureScale({ 30.0f, 30.0f });
		Entity ground = SceneFactory::CreatePlane(m_Scene, device, "Ground", 1.0f, 1.0f, grass);
		ground.Get<TransformComponent>().scale = { 500.0f, 1.0f, 500.0f };
		ground.Get<MeshRendererComponent>().castsShadows = false;   // a 500 m plane in every shadow view would only cost; it RECEIVES shadows
	}

	// ---- moon: a textured sphere
	{
		auto moon = MaterialFactory::CreateLitMaterial("MoonMaterial");
		moon->SetTexture(TextureSlot::Diffuse, ImageLoader::LoadForSlot(*device, "assets/textures/8k_moon.jpg", TextureSlot::Diffuse));
		moon->SetSpecularColor({ 0.2f, 0.2f, 0.2f, 1.0f });
		moon->SetShininess(100.0f);
		m_Moon = SceneFactory::CreateSphere(m_Scene, device, "Moon", 1.0f, 64, moon);
		m_Moon.Get<TransformComponent>().position = { 100.0f, 40.0f, 10.0f };
		m_Moon.Get<TransformComponent>().scale = { 20.0f, 20.0f, 20.0f };
	}

	// ---- a lamp: a point light entity with a small glowing sphere as its child, so the marker always
	// sits exactly where the light is (this is what the old LightSphere class tried to do by hand)
	{
		Entity lamp = SceneFactory::CreatePointLight(m_Scene, "Lamp", { 10.0f, 15.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 300.0f, 40.0f);
		lamp.Get<LightComponent>().castsShadows = true;              // six atlas tiles: the tunnel, the ring and the nanosuit throw shadows
		auto glow = MaterialFactory::CreateEmissiveMaterial("LampGlow");
		glow->SetEmissiveColor({ 1.0f, 0.5f, 1.0f, 1.0f });
		glow->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		Entity marker = SceneFactory::CreateSphere(m_Scene, device, "LampMarker", 1.5f, 16, glow);
		m_Scene.SetParent(marker, lamp);
		marker.Get<TransformComponent>().scale = { 2.0f, 2.0f, 2.0f };
		marker.Get<MeshRendererComponent>().castsShadows = false;
	}

	// ---- shadows: a dim moonlight with cascaded shadows, and a spot light on the ship (H: shadows on/off, J: filter size)
	{
		Entity moonlight = SceneFactory::CreateDirectionalLight(m_Scene, "Moonlight", { -0.4f, -0.8f, 0.45f }, { 0.55f, 0.65f, 1.0f }, 1.2f);
		moonlight.Get<LightComponent>().castsShadows = true;

		Entity spot = m_Scene.CreateEntity("ShipSpot");
		auto& t = spot.Get<TransformComponent>();
		t.position = { -18.0f, 22.0f, 12.0f };
		t.SetForward({ 18.0f, -17.0f, 18.0f });                      // toward the ship at (0, 5, 30)
		auto& l = spot.Add<LightComponent>();
		l.type = LightType::Spot;
		l.color = { 1.0f, 0.9f, 0.7f };
		l.intensity = 400.0f;
		l.range = 50.0f;
		l.innerConeDegrees = 18.0f;
		l.outerConeDegrees = 32.0f;
		l.castsShadows = true;
	}

	// ---- imported models become entities; position them once
	auto place = [&](const char* path, const char* name, DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 scale = { 1, 1, 1 }) {
		Entity e = SceneFactory::Instantiate(m_Scene, m_Loader->LoadModel(path), name);
		if (e)
		{
			e.Get<TransformComponent>().position = position;
			e.Get<TransformComponent>().scale = scale;
		}
		return e;
	};
	place("assets/models/tunnel/future_tunnel.glb",           "Tunnel", { 0.0f, 3.0f, 0.0f }, { 0.4f, 0.4f, 0.4f });
	Entity ship = place("assets/models/ship/dutch_ship_large_02_1k.fbx", "Ship", { 0.0f, 5.0f, 30.0f });
	if (ship) ship.Get<TransformComponent>().SetEulerDegrees(19.0f, 0.0f, 0.0f);
	place("assets/models/shark/scene.gltf",                   "Shark", { 10.0f, 30.0f, -40.0f });
	place("assets/models/ring.gltf",                          "Ring",  { 0.0f, 8.0f, 0.0f }, { 2.0f, 2.0f, 2.0f });
	place("assets/models/nano_textured/nanosuit.obj",         "Nanosuit", { 0.0f, 10.0f, -5.0f });
	place("assets/models/brick_wall/brick_wall.obj",          "Wall",  { 0.0f, 10.0f, 10.0f }, { 5.0f, 5.0f, 5.0f });
	m_LionHead = place("assets/models/lion/lionHead.fbx",     "LionHead", { 0.0f, 5.0f, -20.0f }, { 30.0f, 30.0f, 30.0f });

	// ---- the animated model: kept as an asset for the animation key bindings AND instantiated for drawing
	m_AnimatedSpider = m_Loader->LoadModel("assets/models/cube/cube.fbx");
	if (m_AnimatedSpider)
	{
		const size_t animCount = m_AnimatedSpider->GetAnimationClipCount();
		OutputDebugStringA(("Animated model: " + std::to_string(animCount) + " clip(s)\n").c_str());
		auto animNames = m_AnimatedSpider->GetAnimationClipNames();
		for (size_t i = 0; i < animNames.size(); i++)
			OutputDebugStringA(("    [" + std::to_string(i) + "] " + animNames[i] + "\n").c_str());

		// play clip 0 (the old code asked for clip 1, which does not exist when there is only one clip)
		if (animCount > 0)
			m_AnimatedSpider->PlayAnimation(0, PlaybackMode::Loop);

		Entity animated = SceneFactory::Instantiate(m_Scene, m_AnimatedSpider, "AnimatedCube");
		if (animated) animated.Get<TransformComponent>().position = { 0.0f, 0.0f, -10.0f };
	}

	// ---- the editor edits this scene through this camera
	m_Editor.SetScene(&m_Scene);
	m_Editor.SetCamera(m_CameraController);
}

void Sandbox::OnDetach()
{
}

void Sandbox::OnUpdate(float dt)
{
	Renderer::SetClearColor(0.1f, 0.1f, 0.16f);

	// F1: edit <-> play. (Polled, so not while a text field is being edited.)
	const bool f1Down = Input::IsKeyPressed(VK_F1);
	if (f1Down && !m_F1WasDown && !Application::Get().GetImGuiLayer().WantsTextInput())
		m_Editor.SetEnabled(!m_Editor.IsEnabled());
	m_F1WasDown = f1Down;

	m_CameraController->Update(dt);                   // (the editor decides when the camera listens: Editor::SetCamera)

	// The shortcuts below are polled too: only when the viewport has focus (edit) or always (play).
	if (m_Editor.AcceptsGameInput())
		DetectInput(dt);

	// gameplay: only things that actually change are touched
	m_CurrentRotation += m_Speed * dt;
	if (m_LionHead)
		m_LionHead.Get<TransformComponent>().SetEulerDegrees(20.0f, m_CurrentRotation, 0.0f);

	AnimationSystem::Update(m_Scene, dt);             // advance animations (before transforms are derived)

	Renderer::BeginScene(m_CameraController->GetCamera());
	Renderer::Submit(m_Scene);                        // the renderer reads the persistent scene

	auto button = std::make_shared<UIButton>("Test Button", UIRect(100, 100, 200, 50));
	button->SetNormalColor(UIColor(0.3f, 0.3f, 0.8f, 0.5f));
	Renderer::SubmitUI(button);

	Renderer::EndScene();
}

void Sandbox::OnUIRender()
{
	m_Editor.OnImGui();
}

void Sandbox::OnEvent(DXEngine::Event& event)
{
	m_CameraController->OnEvent(event);
	DXEngine::EventDispatcher dispatcher(event);
	dispatcher.Dispatch<DXEngine::MouseButtonPressedEvent>(DX_BIND_EVENT_FN(Sandbox::OnMouseButtonPressed));
}

bool Sandbox::OnMouseButtonPressed(DXEngine::MouseButtonPressedEvent& e)
{

	// (in edit mode the editor's viewport picks, and selects)
	if (e.GetMouseButton() == VK_LBUTTON && !m_Editor.IsEnabled())
	{
		m_LastMouseX = DXEngine::Input::GetMouseX();
		m_LastMouseY = DXEngine::Input::GetMouseY();

		// Handle picking on left mouse button press
		HandlePicking(m_LastMouseX, m_LastMouseY);
	}

	return false;
}

void Sandbox::CycleLightCulling()
{
	m_Environment = Renderer::GetEnvironment();       // the Renderer panel may have changed it since
	const char* name = "";
	switch (m_Environment.lightCulling)
	{
	case LightCullingMode::Clustered:      m_Environment.lightCulling = LightCullingMode::ClusterHeatmap; name = "cluster heat map (lights per cluster)"; break;
	case LightCullingMode::ClusterHeatmap: m_Environment.lightCulling = LightCullingMode::None;           name = "none: every pixel loops over all lights"; break;
	default:                               m_Environment.lightCulling = LightCullingMode::Clustered;      name = "clustered"; break;
	}
	Renderer::SetEnvironment(m_Environment);
	OutputDebugStringA((std::string("Light culling: ") + name + "\n").c_str());
}

void Sandbox::ToggleShadows()
{
	m_Environment = Renderer::GetEnvironment();
	m_Environment.shadows.enabled = !m_Environment.shadows.enabled;
	Renderer::SetEnvironment(m_Environment);
	OutputDebugStringA(m_Environment.shadows.enabled ? "Shadows: on\n" : "Shadows: off\n");
}

void Sandbox::CyclePcfRadius()
{
	m_Environment = Renderer::GetEnvironment();
	m_Environment.shadows.pcfRadius = (m_Environment.shadows.pcfRadius + 1) % 4;
	Renderer::SetEnvironment(m_Environment);
	const uint32_t r = m_Environment.shadows.pcfRadius;
	OutputDebugStringA(("Shadow filter: " + std::to_string(2 * r + 1) + "x" + std::to_string(2 * r + 1) + " taps\n").c_str());
}

void Sandbox::ToggleStressLights()
{
	if (!m_StressLights.empty())
	{
		for (Entity& e : m_StressLights) m_Scene.DestroyEntity(e);
		m_StressLights.clear();
		OutputDebugStringA("Stress lights: removed\n");
		return;
	}

	// 1000 small lights scattered over the ground around the models. Fixed seed: the same scene every time, so
	// clustered and unclustered frame times can be compared fairly.
	std::mt19937 rng(1234);
	std::uniform_real_distribution<float> spread(-120.0f, 120.0f), height(1.0f, 9.0f), range(8.0f, 16.0f), channel(0.2f, 1.0f);
	m_StressLights.reserve(1000);
	for (int i = 0; i < 1000; ++i)
	{
		m_StressLights.push_back(SceneFactory::CreatePointLight(m_Scene, "StressLight",
			{ spread(rng), height(rng), spread(rng) }, { channel(rng), channel(rng), channel(rng) }, 60.0f, range(rng)));
	}
	OutputDebugStringA("Stress lights: 1000 point lights added\n");
}

void Sandbox::DetectInput(double time)
{
	// Toggle wireframe mode
	static bool wireframeToggled = false;
	if (DXEngine::Input::IsKeyPressed('T'))
	{
		if (!wireframeToggled)
		{
			m_WireframeMode = !DXEngine::Renderer::IsWireframeEnabled();    // ask the renderer: the panel can flip it too
			DXEngine::Renderer::EnableWireframe(m_WireframeMode);
			wireframeToggled = true;
		}
	}
	else
	{
		wireframeToggled = false;
	}

	// L: cycle how pixels find their lights (clustered -> heat map -> all lights). K: 1000 point lights on/off.
	// Use them together with I (statistics): the same scene, with and without clusters, to compare frame times.
	static bool cullingToggled = false;
	if (DXEngine::Input::IsKeyPressed('L'))
	{
		if (!cullingToggled) { CycleLightCulling(); cullingToggled = true; }
	}
	else cullingToggled = false;

	static bool stressToggled = false;
	if (DXEngine::Input::IsKeyPressed('K'))
	{
		if (!stressToggled) { ToggleStressLights(); stressToggled = true; }
	}
	else stressToggled = false;

	// H: shadows on/off. J: shadow filter size (hard, 3x3, 5x5, 7x7 taps). Use with I to see the shadow counters.
	static bool shadowsToggled = false;
	if (DXEngine::Input::IsKeyPressed('H'))
	{
		if (!shadowsToggled) { ToggleShadows(); shadowsToggled = true; }
	}
	else shadowsToggled = false;

	static bool pcfToggled = false;
	if (DXEngine::Input::IsKeyPressed('J'))
	{
		if (!pcfToggled) { CyclePcfRadius(); pcfToggled = true; }
	}
	else pcfToggled = false;

	// Toggle debug info
	static bool debugToggled = false;
	if (DXEngine::Input::IsKeyPressed('I'))
	{
		if (!debugToggled)
		{
			DXEngine::Renderer::EnableDebugInfo(!mDX_DEBUGMode);
			mDX_DEBUGMode = !mDX_DEBUGMode;
			debugToggled = true;
		}
	}
	else
	{
		debugToggled = false;
	}

	// Spider Animation control
	if (m_AnimatedSpider)
	{
		// Switch to next animation with 'N' key
		static bool animSwitchToggled = false;
		if (DXEngine::Input::IsKeyPressed('N'))
		{
			if (!animSwitchToggled)
			{
				size_t animCount = m_AnimatedSpider->GetAnimationClipCount();
				if (animCount > 0)
				{
					m_CurrentAnimationIndex = (m_CurrentAnimationIndex + 1) % animCount;
					m_AnimatedSpider->PlayAnimation(m_CurrentAnimationIndex,
						DXEngine::PlaybackMode::Loop);

					auto names = m_AnimatedSpider->GetAnimationClipNames();
					OutputDebugStringA(("Switched to animation: " +
						names[m_CurrentAnimationIndex] + "\n").c_str());
				}
				animSwitchToggled = true;
			}
		}
		else
		{
			animSwitchToggled = false;
		}

		// Pause/Resume animation with 'P' key
		static bool pauseToggled = false;
		if (DXEngine::Input::IsKeyPressed('P'))
		{
			if (!pauseToggled)
			{
				if (m_IsAnimationPaused)
				{
					m_AnimatedSpider->ResumeAnimation();
					OutputDebugStringA("Animation resumed\n");
				}
				else
				{
					m_AnimatedSpider->PauseAnimation();
					OutputDebugStringA("Animation paused\n");
				}
				m_IsAnimationPaused = !m_IsAnimationPaused;
				pauseToggled = true;
			}
		}
		else
		{
			pauseToggled = false;
		}

		// Change animation speed with '+' and '-' keys
		static bool speedUpToggled = false;
		if (DXEngine::Input::IsKeyPressed(VK_OEM_PLUS) || DXEngine::Input::IsKeyPressed(VK_ADD))
		{
			if (!speedUpToggled && m_AnimatedSpider->GetAnimationController())
			{
				float currentSpeed = m_AnimatedSpider->GetAnimationController()->GetPlaybackSpeed();
				float newSpeed = std::min(currentSpeed + 0.25f, 3.0f);
				m_AnimatedSpider->GetAnimationController()->SetPlaybackSpeed(newSpeed);
				OutputDebugStringA(("Animation speed: " + std::to_string(newSpeed) + "x\n").c_str());
				speedUpToggled = true;
			}
		}
		else
		{
			speedUpToggled = false;
		}

		static bool speedDownToggled = false;
		if (DXEngine::Input::IsKeyPressed(VK_OEM_MINUS) || DXEngine::Input::IsKeyPressed(VK_SUBTRACT))
		{
			if (!speedDownToggled && m_AnimatedSpider->GetAnimationController())
			{
				float currentSpeed = m_AnimatedSpider->GetAnimationController()->GetPlaybackSpeed();
				float newSpeed = std::max(currentSpeed - 0.25f, 0.25f);
				m_AnimatedSpider->GetAnimationController()->SetPlaybackSpeed(newSpeed);
				OutputDebugStringA(("Animation speed: " + std::to_string(newSpeed) + "x\n").c_str());
				speedDownToggled = true;
			}
		}
		else
		{
			speedDownToggled = false;
		}

		// Change playback mode with 'M' key (Loop -> Once -> PingPong)
		static bool modeToggled = false;
		if (DXEngine::Input::IsKeyPressed('M'))
		{
			if (!modeToggled && m_AnimatedSpider->GetAnimationController())
			{
				auto controller = m_AnimatedSpider->GetAnimationController();
				auto currentMode = controller->GetPlaybackMode();

				DXEngine::PlaybackMode newMode;
				std::string modeName;

				switch (currentMode)
				{
				case DXEngine::PlaybackMode::Loop:
					newMode = DXEngine::PlaybackMode::Once;
					modeName = "Once";
					break;
				case DXEngine::PlaybackMode::Once:
					newMode = DXEngine::PlaybackMode::PingPong;
					modeName = "PingPong";
					break;
				case DXEngine::PlaybackMode::PingPong:
					newMode = DXEngine::PlaybackMode::Loop;
					modeName = "Loop";
					break;
				}

				controller->SetPlaybackMode(newMode);
				OutputDebugStringA(("Playback mode: " + modeName + "\n").c_str());
				modeToggled = true;
			}
		}
		else
		{
			modeToggled = false;
		}
	}

	//call update

	return;
}

void Sandbox::HandlePicking(float mouseX, float mouseY)
{
	if (!m_CameraController->GetCamera())
		return;

	const int screenWidth = Application::Get().GetWindow().GetWidth();
	const int screenHeight = Application::Get().GetWindow().GetHeight();
	if (screenWidth <= 0 || screenHeight <= 0)
		return;

	// which ENTITY is under the cursor? (replaces PickingManager + dynamic_cast on Model subclasses)
	PickResult pick = ScenePicking::Pick(m_Scene, *m_CameraController->GetCamera(), mouseX, mouseY, screenWidth, screenHeight);
	if (pick)
		OutputDebugStringA(("Picked: " + pick.entity.Get<NameComponent>().name + "\n").c_str());
}
