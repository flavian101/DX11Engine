#include "App.h"
#include <optional>
App::App(HINSTANCE hInstance, int showWnd)
    :
    window(hInstance, showWnd, L"engine", L"DirectX", 800, 600),
    tri(window.Gfx())

{
    window.Gfx().SetProjection(DirectX::XMMatrixPerspectiveLH(1.0f, 9.0f / 16.0f, 0.5f, 100.0f));
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
    window.Gfx().SetCamera(camera.GetView());
    tri.Draw(window.Gfx());

    window.Gfx().End();
}
