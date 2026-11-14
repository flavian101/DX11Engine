#include "dxpch.h"
#include "ImGuiLayer.h"
#include "Application.h"
#include "renderer/Renderer.h"
#include "RHI/D3D11/D3D11Device.h"
#include "RHI/D3D11/D3D11Texture.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

// imgui_impl_win32.h only declares this one inside '#if 0' (the application is meant to forward-declare it).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace DXEngine
{
    namespace
    {
        // The two D3D11 objects the backend needs, or null when the device is not D3D11.
        RHI::D3D11Device* GetD3D11Device()
        {
            auto device = Renderer::GetDevice();
            if (!device || device->GetAPI() != RHI::GraphicsAPI::DirectX11)
                return nullptr;
            return static_cast<RHI::D3D11Device*>(device.get());
        }
    }

    ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}

    ImGuiLayer::~ImGuiLayer()
    {
        OnDetach();      // normally already done by Application; harmless the second time
    }

    void ImGuiLayer::OnAttach()
    {
        if (m_Active) return;

        RHI::D3D11Device* d3d = GetD3D11Device();
        if (!d3d)
        {
            OutputDebugStringA("ImGuiLayer: only the DirectX 11 backend exists, the UI layer stays inactive\n");
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // the docking branch: panels dock into each other and into the window
        io.ConfigWindowsMoveFromTitleBarOnly = true;           // dragging inside a panel must never move the panel
        io.IniFilename = m_IniFile.c_str();                    // the layout is remembered between runs

        ApplyStyle();

        HWND hwnd = Application::Get().GetWindow().GetHwnd();
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(d3d->GetD3D11Device(), d3d->GetD3D11Context());

        Application::Get().GetWindow().SetNativeMessageHook(
            [](HWND h, UINT msg, WPARAM w, LPARAM l) -> bool
            {
                return ImGui_ImplWin32_WndProcHandler(h, msg, w, l) != 0;
            });

        m_Active = true;
    }

    void ImGuiLayer::OnDetach()
    {
        if (!m_Active) return;
        m_Active = false;

        // stop feeding a context that is about to disappear
        if (Application::HasInstance())
            Application::Get().GetWindow().SetNativeMessageHook(nullptr);

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    UiInputState ImGuiLayer::State() const
    {
        UiInputState s;
        s.active = m_Active;
        s.blockMouseEvents = m_BlockMouseEvents;
        s.mouseEnabled = m_MouseEnabled;
        if (m_Active)
        {
            const ImGuiIO& io = ImGui::GetIO();
            s.wantMouse = io.WantCaptureMouse;
            s.wantKeyboard = io.WantCaptureKeyboard;
        }
        return s;
    }

    void ImGuiLayer::OnEvent(Event& e)
    {
        if (!m_Active) return;

        const UiInputState state = State();
        if (e.IsInCategory(EventCategoryMouse) && ShouldBlockMouseEvent(state))
            e.Handled = true;
        if (e.IsInCategory(EventCategoryKeyboard) && ShouldBlockKeyboardEvent(state))
            e.Handled = true;
    }

    void ImGuiLayer::Begin()
    {
        if (!m_Active) return;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End()
    {
        if (!m_Active) return;

        ImGui::Render();

        // The UI goes on the window itself, over what the renderer left there (the scene, or the editor's cleared background).
        RHI::D3D11Device* d3d = GetD3D11Device();
        if (d3d)
        {
            auto* backBuffer = static_cast<RHI::D3D11Texture*>(d3d->GetBackBuffer());
            ID3D11RenderTargetView* rtv = backBuffer->GetRTV();
            d3d->GetD3D11Context()->OMSetRenderTargets(1, &rtv, nullptr);
        }
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    bool ImGuiLayer::WantsMouse() const    { return ShouldBlockMouseEvent(State()); }
    bool ImGuiLayer::WantsKeyboard() const { return ShouldBlockKeyboardEvent(State()); }
    bool ImGuiLayer::WantsTextInput() const{ return m_Active && ImGui::GetIO().WantTextInput; }

    void ImGuiLayer::SetMouseEnabled(bool enabled)
    {
        m_MouseEnabled = enabled;      // what events and WantsMouse() go by: ImGui's own flags do not stop WantCaptureMouse (see UiInputBlocking.h)
        if (!m_Active) return;
        ImGuiIO& io = ImGui::GetIO();
        const ImGuiConfigFlags off = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
        if (enabled) io.ConfigFlags &= ~off;
        else         io.ConfigFlags |= off;
    }

    void ImGuiLayer::ApplyStyle()
    {
        // A neutral base. How the editor looks is EditorTheme's business (it is applied when the editor draws its first frame),
        // so there is one place to change.
        ImGui::StyleColorsDark();
    }
}
