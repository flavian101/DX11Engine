#pragma once
#include "DXEngine.h"
#include <memory>

// The editor application: one Scene, one camera, and the Editor (docked panels) that edits them.
//
// The scene is built from primitives only (see BuildDefaultScene), so the application starts fast; importing models
// and textures from disk is what the asset system (next milestone) is for.
//
// F1 switches between editing and playing (play = the scene fills the window, no panels).
class EditorLayer : public DXEngine::Layer
{
public:
	EditorLayer();
	~EditorLayer() override;

	void OnAttach() override;
	void OnUpdate(float dt) override;
	void OnUIRender() override;
	void OnEvent(DXEngine::Event& event) override;

private:
	void BuildDefaultScene();

	std::shared_ptr<DXEngine::CameraController> m_CameraController;
	DXEngine::Scene  m_Scene;
	DXEngine::Editor m_Editor;                 // edits m_Scene through m_CameraController
	bool m_F1WasDown = false;
};
