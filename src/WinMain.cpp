#include "Window.h"
#include <optional>
#include "Application.h"

int CALLBACK WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow)
{
	


	return DXEngine::Application(hInstance, nCmdShow).createLoop();



	

	
}

