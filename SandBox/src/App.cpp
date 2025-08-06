#include "DXEngine.h"
#include "Sandbox.h"

class App: public DXEngine::Application
{
public:
	App()
	{
		PushLayer(new Sandbox());

	}
};

DXEngine::Application* DXEngine::CreateApplication()
{
	return new App();
}