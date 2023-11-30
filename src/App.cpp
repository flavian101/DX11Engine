#include "App.h"
#include <optional>
App::App(HINSTANCE hInstance, int showWnd)
    :
    window(hInstance, showWnd, L"engine", L"DirectX", 800, 600),
    tri(window.Gfx())

{
   // m(window.Gfx());
}

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
    const float t = timer.Peek();
    window.Gfx().ClearDepthColor(0.1f, 0.1f, 0.16f);
    tri.Draw(window.Gfx());

    window.Gfx().End();
}
