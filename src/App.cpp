#include "App.h"
#include <optional>
App::App(HINSTANCE hInstance, int showWnd)
    :
    window(hInstance, showWnd, L"engine", L"DirectX", 800, 600),
    wnd(hInstance, showWnd, L"game", L"game", 300, 300)
{}

int App::createLoop()
{
    while (true)
    {
        //process all messages pending 
        if (const auto ecode = Window::ProcessMessages())
        {
            //if return optional has value, means we'are exiting the program by returning the exit code
            return *ecode;
        }
        Render();
	}
}

void App::Render()
{
}
