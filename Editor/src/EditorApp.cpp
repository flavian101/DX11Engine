#include "DXEngine.h"
#include "EditorLayer.h"
#include "editor/ImGuiLayer.h"

#include "Main.h"

// The editor application: the engine's Application plus the things only an editor has.
//   * the ImGui layer (top overlay) and the two Application hooks that put its frame around the panels
//   * the editor's input policy: middle mouse button = the camera owns the mouse, Escape = quit
// (They used to live in DXEngine::Application itself, which tied the engine library to ImGui; a game built on the
// engine has neither.)
class EditorApp : public DXEngine::Application
{
public:
	EditorApp()
		: Application()
	{
		// An overlay is always above the layers, so it is the first to see events whatever the push order.
		m_ImGui = new DXEngine::ImGuiLayer();
		PushOverlay(m_ImGui);
		PushLayer(new EditorLayer());
	}

protected:
	void OnUIFrameBegin() override { m_ImGui->Begin(); }
	void OnUIFrameEnd() override { m_ImGui->End(); }

	void OnApplicationEvent(DXEngine::Event& e) override
	{
		DXEngine::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<DXEngine::KeyPressedEvent>([this](DXEngine::KeyPressedEvent& k)
		{
			if (k.GetKeyCode() != VK_ESCAPE) return false;
			if (m_ImGui->WantsTextInput())          // Escape belongs to the text field being edited
				return false;
			GetWindow().QuitWindow();
			return true;
		});
		dispatcher.Dispatch<DXEngine::MouseButtonPressedEvent>([this](DXEngine::MouseButtonPressedEvent& m)
		{
			if (m.GetMouseButton() != VK_MBUTTON) return false;
			if (m_ImGui->WantsMouse())              // the click is on a panel, not on the scene
				return false;
			GetWindow().CaptureCursor();
			m_ImGui->SetMouseEnabled(false);        // the camera owns the mouse now: the pointer is re-centred every move,
			return true;                            // panels must not react to it
		});
		dispatcher.Dispatch<DXEngine::MouseButtonReleasedEvent>([this](DXEngine::MouseButtonReleasedEvent& m)
		{
			if (m.GetMouseButton() != VK_MBUTTON) return false;
			GetWindow().ReleaseCursor();
			m_ImGui->SetMouseEnabled(true);
			return true;
		});
	}

private:
	DXEngine::ImGuiLayer* m_ImGui = nullptr;     // owned by the layer stack
};

DXEngine::Application* DXEngine::CreateApplication()
{
	return new EditorApp();
}
