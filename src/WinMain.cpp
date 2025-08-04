#include "Window.h"
#include <optional>
#include "App.h"

int CALLBACK WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow)
{
	


	return DXEngine::App(hInstance, nCmdShow).createLoop();



	

	
}

