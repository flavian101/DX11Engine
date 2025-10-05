#pragma once
#include "DXEngine.h"
#include <memory>
#include "Ground.h"
#include "Ball.h"
#include "SkySphere.h"
#include "LightSphere.h"




class Sandbox :public DXEngine::Layer
{
public:
	Sandbox();;
	virtual ~Sandbox();

	virtual void OnAttach()override;
	virtual void OnDetach()override;
	void OnUpdate(DXEngine::FrameTime dt) override;
	virtual void OnUIRender() override;
	void OnEvent(DXEngine::Event& event) override;

	bool OnMouseButtonPressed(DXEngine::MouseButtonPressedEvent& e);


	void DetectInput(double time);


	void HandlePicking(float mouseX, float mouseY);
	void InitializePicking();
private:
	//bool OnMouseButtonPressed(DXEngine::MouseButtonPressedEvent& e);

private:
	std::shared_ptr<DXEngine::CameraController> m_CameraController;
	std::shared_ptr<DXEngine::Ground> m_Ground;
	std::shared_ptr<DXEngine::SkySphere> m_Sky;
	std::shared_ptr<DXEngine::Ball> m_Moon;
	std::shared_ptr<DXEngine::LightSphere> m_Light;
	std::shared_ptr<DXEngine::ModelLoader> m_Loader;
	std::shared_ptr<DXEngine::Model> m_Ship;
	std::shared_ptr<DXEngine::Model> m_Table;
	std::shared_ptr<DXEngine::Model> m_LionHead;
	std::shared_ptr<DXEngine::Model> m_Tunnel;
	std::shared_ptr<DXEngine::Model> m_Shark;
	std::shared_ptr<DXEngine::Model> m_Ring;
	std::shared_ptr<DXEngine::Model> m_Wall;
	std::shared_ptr<DXEngine::Model> m_AnimatedSpider;


	float m_Speed = 10.0f;
	float m_CurrentRotation = 0.0f;

	//Window wnd;
	std::unique_ptr<DXEngine::PickingManager> m_PickingManager;

	// Mouse state for picking
	float m_LastMouseX = 0.0f;
	float m_LastMouseY = 0.0f;
	bool m_WireframeMode = false;
	bool mDX_DEBUGMode = false;

	int m_CurrentAnimationIndex = 0;
	bool m_IsAnimationPaused = false;
};

