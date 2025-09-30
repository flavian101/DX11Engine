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

	m_Ship = m_Loader->LoadModel("assets/models/UFO/Rigged_Modular UFO 2.8.glb.gltf");
	m_Table = m_Loader->LoadModel("assets/models/eShip/Intergalactic Spaceship_Blender_2.8_Packed textures.glb");
	m_LionHead = m_Loader->LoadModel("assets/models/lion/lionHead.fbx");
	m_Tunnel = m_Loader->LoadModel("assets/models/tunnel/future_tunnel.glb");
	m_Shark = m_Loader->LoadModel("assets/models/shark/scene.gltf");
	m_Ring = m_Loader->LoadModel("assets/models/ring.gltf");
	m_Wall = m_Loader->LoadModel("assets/models/brick_wall/brick_wall.obj");

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

	//// Ground
	//if (m_Ground)
	//{
	//	m_Ground->SetScale({ 500.0f, 10.0f, 500.0f });
	//	m_Ground->SetTranslation({ 0.0f,0.0f, 0.0f });
	//	DXEngine::Renderer::Submit(m_Ground);
	//}
	//
	//if (m_Tunnel)
	//{
	//	m_Tunnel->SetTranslation({ 0.0f, 3.0f, 0.0f });
	//	m_Tunnel->SetScale({ 0.4f, 0.4f, 0.4f });
	//	DXEngine::Renderer::Submit(std::dynamic_pointer_cast<DXEngine::Model>(m_Tunnel));
	//}
	//
	//if (m_Ship)
	//{
	//	m_Ship->SetScale({ 3.0f, 3.0f, 3.0f });
	//	m_Ship->SetTranslation({ 0.0f, 4.0f, 15.0f });
	//	DXEngine::Renderer::Submit(std::dynamic_pointer_cast<DXEngine::Model>(m_Ship));
	//}
	//
	//if (m_Table)
	//{
	//	m_Table->SetScale({ 0.04f, 0.04f, 0.04f });
	//	m_Table->SetTranslation({ 0.0f, 30.0f, -5.0f });
	//	m_Table->SetRotation({ -20.0f, 0.0f, 0.0f, 0.0f });
	//
	//	DXEngine::Renderer::Submit(std::dynamic_pointer_cast<DXEngine::Model>(m_Table));
	//}
	//
	//if (m_Shark)
	//{
	//	m_Shark->SetTranslation({ 10.0f, 30.0f, -40.0f });
	//	DXEngine::Renderer::Submit(std::dynamic_pointer_cast<DXEngine::Model>(m_Shark));
	//}
	//
	//if (m_LionHead)
	//{
	//	m_LionHead->SetScale({ 30.0f, 30.0f, 30.0f });
	//	m_LionHead->SetTranslation({ 0.0f, 5.0f, -20.0f });
	//	m_CurrentRotation += m_Speed * dt;
	//	m_LionHead->SetRotation({ 20.0f, m_CurrentRotation, 0.0f, 0.0f });
	//	DXEngine::Renderer::Submit(std::dynamic_pointer_cast<DXEngine::Model>(m_LionHead));
	//}
	//
	//if (m_Moon)
	//{
	//	m_Moon->SetTranslation({ 100.0f, 40.0f, 10.0f });
	//	m_Moon->SetScale({ 20.f, 20.0f, 20.0f });
	//	DXEngine::Renderer::Submit(m_Moon);
	//}
	//
	//if (m_Light)
	//{
	//	m_Light->SetTranslation({ 10.0f, 15.0f, 0.0f });
	//	m_Light->SetScale({ 2.0f, 2.0f, 2.0f });
	//	DXEngine::Renderer::Submit(m_Light);
	//}

	if (m_Ring)
	{
		m_Ring->SetTranslation({ 0.0f, 8.0f, 0.0f });
		m_Ring->SetScale({ 2.0f, 2.0f, 2.0f });
		DXEngine::Renderer::Submit(std::dynamic_pointer_cast<DXEngine::Model>(m_Ring));
	}
	//if (m_Wall)
	//{
	//	m_Wall->SetTranslation({ 0.0f, 10.0f, 10.0f });
	//	m_Wall->SetScale({ 5.0f, 5.0f, 5.0f });
	//	DXEngine::Renderer::Submit(std::dynamic_pointer_cast<DXEngine::Model>(m_Wall));
	//}

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
	if (DXEngine::Input::IsKeyPressed('T'))
	{
		static bool wireframeToggled = false;
		if (!wireframeToggled)
		{
			DXEngine::Renderer::EnableWireframe(!m_WireframeMode);
			m_WireframeMode = !m_WireframeMode;
			wireframeToggled = true;
		}
	}
	else
	{
		static bool wireframeToggled = false;
		wireframeToggled = false;
	}

	// Toggle debug info
	if (DXEngine::Input::IsKeyPressed('I'))
	{
		static bool debugToggled = false;
		if (!debugToggled)
		{
			DXEngine::Renderer::EnableDebugInfo(!mDX_DEBUGMode);
			mDX_DEBUGMode = !mDX_DEBUGMode;
			debugToggled = true;
		}
	}
	else
	{
		static bool debugToggled = false;
		debugToggled = false;
	}

	//call update

	return;
}
void Sandbox::InitializePicking()
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