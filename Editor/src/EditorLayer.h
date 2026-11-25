#pragma once
#include "DXEngine.h"
#include "editor/Editor.h"
#include <memory>

// The editor application: one Scene, one camera, and the Editor (docked panels) that edits them.
//
// The starting scene is the file last opened or saved (Editor::OpenLastScene). Before any file exists, BuildDefaultScene builds
// one in code: a sky, a lit ground, spheres and a lamp.
//
// F1 switches between editing and playing. Play saves the scene in memory, fills the window with it and runs the game (m_Game:
// the systems that exist only while playing); Stop puts the scene back exactly as it was. See Editor::Play / Editor::Stop.
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
	DXEngine::GameRuntime m_Game;              // the gameplay: runs only between Play and Stop, on the live scene
	DXEngine::Editor m_Editor;                 // edits m_Scene through m_CameraController; runs m_Game while playing
	bool m_F1WasDown = false;
};
