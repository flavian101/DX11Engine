#include "Sandbox.h"


Sandbox::Sandbox()
	:Layer("sanbox")
{}

Sandbox::~Sandbox()
{
}

void Sandbox::OnAttach()
{
	DXEngine::Renderer::InitLightManager();

	m_CameraController = std::make_shared<DXEngine::CameraController>();

	m_Ground = std::make_shared<DXEngine::Ground>();
	m_Moon = std::make_shared<DXEngine::Ball>();
	m_Sky = std::make_shared<DXEngine::SkySphere>();
	m_Light = std::make_shared<DXEngine::LightSphere>();
	m_Loader = std::make_shared<DXEngine::ModelLoader>();

//	m_Ship = m_Loader->LoadModel("assets/models/UFO/Rigged_Modular UFO 2.8.glb.gltf");
	m_Table = m_Loader->LoadModel("assets/models/eShip/Intergalactic Spaceship_Blender_2.8_Packed textures.glb");
	m_LionHead = m_Loader->LoadModel("assets/models/lion/lionHead.fbx");
	m_Tunnel = m_Loader->LoadModel("assets/models/tunnel/future_tunnel.glb");
	m_Shark = m_Loader->LoadModel("assets/models/shark/scene.gltf");
	m_Ring = m_Loader->LoadModel("assets/models/ring.gltf");
	m_Wall = m_Loader->LoadModel("assets/models/brick_wall/brick_wall.obj");
	m_AnimatedSpider = m_Loader->LoadModel("assets/models/horse/source/Horse.fbx");
	if (m_AnimatedSpider)
	{
		OutputDebugStringA("Spaceship loaded with animations!\n");

		// Print animation info
		size_t animCount = m_AnimatedSpider->GetAnimationClipCount();
		OutputDebugStringA(("  Animations: " + std::to_string(animCount) + "\n").c_str());

		auto animNames = m_AnimatedSpider->GetAnimationClipNames();
		for (size_t i = 0; i < animNames.size(); i++)
		{
			OutputDebugStringA(("    [" + std::to_string(i) + "] " +
				animNames[i] + "\n").c_str());
		}

		// Play first animation if available
		if (animCount > 0)
		{
			m_AnimatedSpider->PlayAnimation(1, DXEngine::PlaybackMode::Loop);
			OutputDebugStringA(("  Playing: " + animNames[0] + "\n").c_str());
		}

		// Print skeleton info
		if (auto skeleton = m_AnimatedSpider->GetSkeleton())
		{
			OutputDebugStringA(("  Bones: " +
				std::to_string(skeleton->GetBoneCount()) + "\n").c_str());
		}
	}
	else
	{
		OutputDebugStringA("  spider loaded but has no animations\n");
	}
	InitializePicking();
}

void Sandbox::OnDetach()
{
}

void Sandbox::OnUpdate(DXEngine::FrameTime dt)
{
	DXEngine::Renderer::SetClearColor(0.1f, 0.1f, 0.16f);

	//window.().ClearDepthColor(0.1f, 0.1f, 0.16f);
	m_CameraController->Update(dt);
	DXEngine::Renderer::BeginScene(m_CameraController->GetCamera());
	DetectInput(dt);

	if (m_Sky)
	{
		// Position sky sphere at camera position
		DirectX::XMFLOAT3 camPos = {
			DirectX::XMVectorGetX(m_CameraController->GetCamera()->GetPos()),
			DirectX::XMVectorGetY(m_CameraController->GetCamera()->GetPos()),
			DirectX::XMVectorGetZ(m_CameraController->GetCamera()->GetPos())
		};
		m_Sky->SetTranslation(camPos);
		m_Sky->SetScale({ 50.0f, 50.0f, 50.0f });
		DXEngine::Renderer::Submit(m_Sky);
	
	}

	// Ground
	if (m_Ground)
	{
		m_Ground->SetScale({ 500.0f, 10.0f, 500.0f });
		m_Ground->SetTranslation({ 0.0f,0.0f, 0.0f });
		DXEngine::Renderer::Submit(m_Ground);
	}
	
	if (m_Tunnel)
	{
		m_Tunnel->SetTranslation({ 0.0f, 3.0f, 0.0f });
		m_Tunnel->SetScale({ 0.4f, 0.4f, 0.4f });
		DXEngine::Renderer::Submit(m_Tunnel);
	}
	
	//if (m_Ship)
	//{
	//	m_Ship->SetScale({ 3.0f, 3.0f, 3.0f });
	//	m_Ship->SetTranslation({ 0.0f, 4.0f, 15.0f });
	//	DXEngine::Renderer::Submit(m_Ship);
	//}
	
	if (m_Table)
	{
		m_Table->SetScale({ 0.04f, 0.04f, 0.04f });
		m_Table->SetTranslation({ 0.0f, 30.0f, -5.0f });
		m_Table->SetRotation({ -20.0f, 0.0f, 0.0f, 0.0f });
	
		DXEngine::Renderer::Submit(m_Table);
	}
	
	if (m_Shark)
	{
		m_Shark->SetTranslation({ 10.0f, 30.0f, -40.0f });
		DXEngine::Renderer::Submit(m_Shark);
	}
	
	if (m_LionHead)
	{
		m_LionHead->SetScale({ 30.0f, 30.0f, 30.0f });
		m_LionHead->SetTranslation({ 0.0f, 5.0f, -20.0f });
		m_CurrentRotation += m_Speed * dt;
		m_LionHead->SetRotation({ 20.0f, m_CurrentRotation, 0.0f, 0.0f });
		DXEngine::Renderer::Submit(m_LionHead);
	}
	
	if (m_Moon)
	{
		m_Moon->SetTranslation({ 100.0f, 40.0f, 10.0f });
		m_Moon->SetScale({ 20.f, 20.0f, 20.0f });
		DXEngine::Renderer::Submit(m_Moon);
	}
	
	if (m_Light)
	{
		m_Light->SetTranslation({ 10.0f, 15.0f, 0.0f });
		m_Light->SetScale({ 2.0f, 2.0f, 2.0f });
		DXEngine::Renderer::Submit(m_Light);
	}

	if (m_AnimatedSpider)
	{
		// Update animation
		m_AnimatedSpider->Update(dt);

		// Set transform
		m_AnimatedSpider->SetScale({ 1.0f, 1.0f, 1.0f });
		m_AnimatedSpider->SetTranslation({ 0.0f, 0.0f, -10.0f });

		m_CurrentRotation += m_Speed * dt;
		//m_AnimatedSpider->SetRotation({ 0.0f, m_CurrentRotation, 0.0f, 0.0f });

		// Submit for rendering
		DXEngine::Renderer::Submit(m_AnimatedSpider);

		// Debug info (can be removed later)
		if (mDX_DEBUGMode && m_AnimatedSpider->IsAnimating())
		{
			float animTime = m_AnimatedSpider->GetAnimationTime();
			float normalizedTime = m_AnimatedSpider->GetAnimationNormalizedTime();

		}
	}
	else if (m_AnimatedSpider)  // Fallback to non-animated version
	{
		m_AnimatedSpider->SetScale({ 1.0f, 1.0f, 1.0f });
		m_AnimatedSpider->SetTranslation({ 0.0f, 0.0f, -10.0f });
		m_AnimatedSpider->SetRotation({ -20.0f, 0.0f, 0.0f, 0.0f });
		DXEngine::Renderer::Submit(m_AnimatedSpider);
	}

	if (m_Ring)
	{
		m_Ring->SetTranslation({ 0.0f, 8.0f, 0.0f });
		m_Ring->SetScale({ 2.0f, 2.0f, 2.0f });
		DXEngine::Renderer::Submit(m_Ring);
	}
	if (m_Wall)
	{
		m_Wall->SetTranslation({ 0.0f, 10.0f, 10.0f });
		m_Wall->SetScale({ 5.0f, 5.0f, 5.0f });
		DXEngine::Renderer::Submit(m_Wall);
	}

	auto button = std::make_shared<DXEngine::UIButton>("Test Button", DXEngine::UIRect::UIRect(100, 100, 200, 50));
	button->SetNormalColor(DXEngine::UIColor::UIColor(0.3f, 0.3f, 0.8f, 0.5f));
	// Submit to renderer
	DXEngine::Renderer::SubmitUI(button);
	
	DXEngine::Renderer::EndScene();

}

void Sandbox::OnUIRender()
{

	
}

void Sandbox::OnEvent(DXEngine::Event& event)
{
	m_CameraController->OnEvent(event);
	DXEngine::EventDispatcher dispatcher(event);
	dispatcher.Dispatch<DXEngine::MouseButtonPressedEvent>(DX_BIND_EVENT_FN(Sandbox::OnMouseButtonPressed));
}

bool Sandbox::OnMouseButtonPressed(DXEngine::MouseButtonPressedEvent& e)
{

	if (e.GetMouseButton() == VK_LBUTTON)
	{
		m_LastMouseX = DXEngine::Input::GetMouseX();
		m_LastMouseY = DXEngine::Input::GetMouseY();

		// Handle picking on left mouse button press
		HandlePicking(m_LastMouseX, m_LastMouseY);
	}

	return false;
}
void Sandbox::DetectInput(double time)
{
	// Toggle wireframe mode
	static bool wireframeToggled = false;
	if (DXEngine::Input::IsKeyPressed('T'))
	{
		if (!wireframeToggled)
		{
			DXEngine::Renderer::EnableWireframe(!m_WireframeMode);
			m_WireframeMode = !m_WireframeMode;
			wireframeToggled = true;
		}
	}
	else
	{
		wireframeToggled = false;
	}

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
}void Sandbox::InitializePicking()
{
	m_PickingManager = std::make_unique<DXEngine::PickingManager>();

	// Register pickable objects - cast to InterfacePickable interface
	if (m_Moon)
		m_PickingManager->RegisterPickable(m_Moon);

	if (m_Ground)
		m_PickingManager->RegisterPickable(m_Ground);

	if (m_Light)
		m_PickingManager->RegisterPickable(m_Light);

	// Don't register sky sphere as it should not be pickable
	if (m_Sky)
		m_Sky->SetPickable(false);
}

void Sandbox::HandlePicking(float mouseX, float mouseY)
{
	if (!m_PickingManager || !m_CameraController->GetCamera()) 
		return;

	// Get window dimensions
	//RECT clientRect;
	////GetClientRect(window.GetHwnd(), &clientRect); TO-DO
	//int screenWidth = clientRect.right - clientRect.left;
	//int screenHeight = clientRect.bottom - clientRect.top;
	

	int screenWidth = 1270;
		int screenHeight = 720;
	// Perform picking
	DXEngine::HitInfo hit = m_PickingManager->Pick(mouseX, mouseY, screenWidth, screenHeight, *m_CameraController->GetCamera());

	if (hit.Hit)  // Note: lowercase 'hit'
	{
		// Object was picked
		auto pickedObject = m_PickingManager->GetPickedObject();
		if (pickedObject)
		{
			// Since all your objects inherit from Model, cast to Model first
			// Then check the actual type using the raw pointer approach
			DXEngine::Model* modelPtr = static_cast<DXEngine::Model*>(hit.ObjectPtr);
			if (modelPtr)
			{
				// Check what type of object was picked
				if (dynamic_cast<DXEngine::Ball*>(modelPtr))
				{
					std::cout << "Ball was picked" << std::endl;
				}
				else if (dynamic_cast<DXEngine::Ground*>(modelPtr))
				{
					std::cout << "triangle was picked" << std::endl;
				}
				else if (dynamic_cast<DXEngine::LightSphere*>(modelPtr))
				{
					std::cout << "light sphere was picked" << std::endl;
				}
			}
		}
	}
}