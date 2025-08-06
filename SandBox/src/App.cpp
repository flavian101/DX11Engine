#include "DXEngine.h"
#include "Sandbox.h"

#include "Main.h"


class App: public DXEngine::Application
{
public:
	App()
		:Application()
	{
		PushLayer(new Sandbox());

	}
	~App()
	{

	}
};

DXEngine::Application* DXEngine::CreateApplication()
{
	return new App();
}