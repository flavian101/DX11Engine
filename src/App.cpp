#include "App.h"
#include <optional>
#include "Event/Event.h"
#include <functional>


#define BIND_EVENT_FN(fn) std::bind(&App::fn, this, std::placeholders::_1)

App::App(HINSTANCE hInstance, int showWnd)
    :
    window(hInstance, showWnd, L"engine", L"DirectX", 1270, 720),
	camera(std::make_shared<Camera>(1.0f, 9.0f / 16.0f, 0.5f, 100.0f)),
    tri(window.Gfx()),
	ball(window.Gfx()),
	sky(window.Gfx())
{
	window.SetEventCallback([this](Event& e) { OnEvent(e); });

	window.Gfx().SetCamera(camera);
	m_Light = std::make_shared<LightSphere>(window.Gfx());
   // m(window.Gfx());

}

App::~App()
{
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
	
	DetectInput(t);


	sky.Draw(window.Gfx(), camera->GetPos());
    tri.Draw(window.Gfx(),camera->GetPos(),camera->GetTarget());
	m_Light->Draw(window.Gfx());

	ball.SetPos(XMMatrixTranslation(10.0f, 50.0f, 0.0f));
	ball.Draw(window.Gfx(), camera->GetPos(), camera->GetTarget());
	
	
    window.Gfx().End();
	
}

void App::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	// Route each type of event to the correct handler:
	dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResize));
	dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(OnKeyPressed));
}

bool App::OnWindowResize(WindowResizeEvent& e)
{
	//window.Gfx().Resize(e.GetWidth(), e.GetHeight());
	return true;
}

bool App::OnKeyPressed(KeyPressedEvent& e)
{
	switch (e.GetKeyCode())
	{
	case VK_ESCAPE:
		window.QuitWindow();
		return true;
	case 'W':
		//camera->MoveForward();
		return true;
		// … handle other keys …
	}
	return false;  
}

void App::DetectInput(double time)
{

	camera->UpdateCamera();
	//call update

	return;
}
