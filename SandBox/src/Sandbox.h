#pragma once
#include "DXEngine.h"
#include <memory>
#include <vector>

// A test scene for the engine. Everything here is an ENTITY in a Scene: a thing is what components it has
// (see docs/DESIGN.md). Nothing is re-submitted object by object every frame any more.
class Sandbox : public DXEngine::Layer
{
public:
	Sandbox();
	virtual ~Sandbox();

	virtual void OnAttach() override;
	virtual void OnDetach() override;
	void OnUpdate(float dt) override;
	virtual void OnUIRender() override;
	void OnEvent(DXEngine::Event& event) override;

	bool OnMouseButtonPressed(DXEngine::MouseButtonPressedEvent& e);
	void DetectInput(double time);
	void HandlePicking(float mouseX, float mouseY);
	void ToggleStressLights();
	void CycleLightCulling();
	void ToggleShadows();
	void CyclePcfRadius();

private:
	std::shared_ptr<DXEngine::CameraController> m_CameraController;

	DXEngine::Scene m_Scene;                       // the world
	DXEngine::Entity m_LionHead;                   // kept because the sample spins it
	DXEngine::Entity m_Moon;

	// importer output. Loaded models are assets; Instantiate() turns them into entities. The animated one is also
	// kept here because the key bindings in DetectInput control its animation clips.
	std::shared_ptr<DXEngine::ModelLoader> m_Loader;
	std::shared_ptr<DXEngine::Model> m_AnimatedSpider;

	// Light culling A/B + stress test (keys L and K, see DetectInput)
	DXEngine::RenderEnvironment m_Environment;      // the environment last given to the renderer
	std::vector<DXEngine::Entity> m_StressLights;   // 1000 point lights, spawned / removed with K

	float m_Speed = 10.0f;
	float m_CurrentRotation = 0.0f;

	float m_LastMouseX = 0.0f;
	float m_LastMouseY = 0.0f;
	bool m_WireframeMode = false;
	bool mDX_DEBUGMode = false;

	int m_CurrentAnimationIndex = 0;
	bool m_IsAnimationPaused = false;
};
