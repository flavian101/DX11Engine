#include "DXEngine.h"
#include "EditorLayer.h"

#include "Main.h"

class EditorApp : public DXEngine::Application
{
public:
	EditorApp()
		: Application()
	{
		PushLayer(new EditorLayer());
	}
};

DXEngine::Application* DXEngine::CreateApplication()
{
	return new EditorApp();
}
