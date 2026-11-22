#include "dxpch.h"
#include "Renderer.h"

#include "Core/FrameContext.h"
#include "Core/RenderGraph.h"
#include "Core/RenderView.h"
#include "Cache/ResourceCache.h"
#include "MaterialSystem.h"
#include "ShaderCache.h"
#include "Passes/ClearPass.h"
#include "Core/GpuProfiler.h"
#include "Passes/DepthPrepass.h"
#include "Passes/ForwardPass.h"
#include "Passes/ShadowPass.h"
#include "Systems/LightSystem.h"
#include "Systems/SceneExtraction.h"
#include "Systems/ShadowSystem.h"

#include "RHI/GraphicsDevice.h"
#include "camera/Camera.h"
#include "scene/Scene.h"
#include "scene/TransformSystem.h"
#include <cassert>
#include <algorithm>

namespace DXEngine
{
    namespace
    {
        Renderer* s_Primary = nullptr;

        void Log(const char* msg) { OutputDebugStringA(msg); }

        // The colour the window is cleared to while the first view of a frame draws into a texture (nothing in the graph
        // touches the back buffer then). The editor covers all of it with its panels; this is what shows between frames
        // of a resize.
        constexpr float kWindowClear[4] = { 0.09f, 0.09f, 0.10f, 1.0f };
    }

    // Everything the renderer owns that is not a view, in ONE place. Created by Init, destroyed by Shutdown.
    struct Renderer::Impl
    {
        RenderServices services;                         // long-lived, shared by every view
        std::shared_ptr<RHI::ICommandBuffer> cmd;

        FrameContext frame;                              // the blackboard: refilled for every view that is drawn
        RenderGraph  graph;                              // GPU passes

        SceneExtraction extraction;                      // CPU systems, run by Render
        ShadowSystem    shadows;                         // BEFORE lights: the light buffers point at its views
        LightSystem     lights;
        RenderEnvironment environment;                   // ambient / exposure / gamma

        float time = 0.0f;
        float lastFrameTime = 0.0f;
        float frameDelta = 0.0f;
        int   viewsThisFrame = 0;                        // reset by Present: the first view of a frame is the timed one

        // the BeginScene / Submit / EndScene sequence of the main view
        bool inScene = false;
        std::shared_ptr<Camera> pendingCamera;
        Scene* pendingScene = nullptr;

        bool debugInfo = false;
        bool wireframe = false;
    };

    // ------------------------------------------------------------------ lifetime
    Renderer::Renderer()
        : m_Main(new RenderView(this, "Main"))
    {
        m_Views.push_back(m_Main.get());
    }

    Renderer::~Renderer()
    {
        Shutdown();
        for (RenderView* view : m_Views)      // views that outlive us must not call back into a dead renderer
            view->m_Owner = nullptr;
        m_Views.clear();
    }

    bool Renderer::HasInstance() { return s_Primary != nullptr; }
    Renderer& Renderer::Get() { return *s_Primary; }
    std::shared_ptr<RHI::IGraphicsDevice> Renderer::PrimaryDevice()
    {
        return s_Primary ? s_Primary->GetDevice() : std::shared_ptr<RHI::IGraphicsDevice>();
    }

    bool Renderer::IsInitialized() const { return m_Impl != nullptr; }

    void Renderer::UnregisterView(RenderView* view)
    {
        m_Views.erase(std::remove(m_Views.begin(), m_Views.end(), view), m_Views.end());
    }

    bool Renderer::Init(void* windowHandle, int width, int height)
    {
        if (m_Impl) Shutdown();
        m_Impl = std::make_unique<Impl>();
        Impl& g = *m_Impl;
        RenderServices& s = g.services;

        // 1) device
        s.device = std::shared_ptr<RHI::IGraphicsDevice>(
            RHI::GraphicsDeviceFactory::Create(RHI::GraphicsAPI::DirectX11));
        if (!s.device || !s.device->Initialize(windowHandle, width, height))
        {
            Log("ERROR: Renderer::Init - device creation failed\n");
            m_Impl.reset();
            return false;
        }

        // 2) services (order matters: shaders and materials need the device; materials need shaders)
        s.resources = std::make_shared<ResourceCache>(s.device);
        s.shaders   = std::make_shared<ShaderCache>(s.device);
        s.materials = std::make_shared<MaterialSystem>(s.device, s.shaders);
        if (!s.resources->Initialize() || !s.shaders->Initialize() || !s.materials->Initialize() ||
            !g.shadows.Initialize(s) || !g.lights.Initialize(s))
        {
            Log("ERROR: Renderer::Init - service initialisation failed\n");
            m_Impl.reset();
            return false;
        }

        g.cmd = s.device->CreateCommandBuffer();
        g.frame.services = &g.services;

        // GPU timing per pass (the render graph opens a scope for each). Optional: without queries the renderer runs the same.
        s.profiler = std::make_shared<GpuProfiler>();
        if (!s.profiler->Initialize(s.device))
            Log("WARNING: Renderer::Init - GPU timing queries are not available\n");

        // 3) the frame: passes run in the order they are added
        g.graph.AddPass<ShadowPass>("shadow");          // first: every lit draw below samples the atlas it fills
        g.graph.AddPass<ClearPass>("clear");
        g.graph.AddPass<DepthPrepass>("depth prepass"); // after the clear (it needs the cleared depth), before every colour pass
        g.graph.AddPass<ForwardPass>("background",  ForwardPass::Queue::Background);
        g.graph.AddPass<ForwardPass>("opaque",      ForwardPass::Queue::Opaque);
        g.graph.AddPass<ForwardPass>("transparent", ForwardPass::Queue::Transparent);

        if (!g.graph.Compile() || !g.graph.Initialize(g.frame))
        {
            Log("ERROR: Renderer::Init - render graph setup failed\n");
            m_Impl.reset();
            return false;
        }
        s_Primary = this;
        Log("Renderer initialised\n");
        return true;
    }

    void Renderer::Shutdown()
    {
        if (!m_Impl) return;
        Impl& g = *m_Impl;

        for (RenderView* view : m_Views)       // textures are GPU objects: they go before the device does (the views stay usable, empty)
            view->ReleaseTargets();

        g.graph.Shutdown();
        g.services.profiler.reset();
        g.services.resources->Clear();
        g.cmd.reset();
        g.services.materials.reset();
        g.services.shaders.reset();
        g.services.resources.reset();
        g.services.device->Shutdown();     // device last: everything above may hold GPU objects
        g.services.device.reset();
        m_Impl.reset();
        if (s_Primary == this) s_Primary = nullptr;
    }

    std::shared_ptr<RHI::IGraphicsDevice> Renderer::GetDevice() const
    {
        return m_Impl ? m_Impl->services.device : std::shared_ptr<RHI::IGraphicsDevice>();
    }

    void Renderer::SetEnvironment(const RenderEnvironment& environment)
    {
        if (m_Impl) m_Impl->environment = environment;
    }

    RenderEnvironment Renderer::GetEnvironment() const
    {
        return m_Impl ? m_Impl->environment : RenderEnvironment();
    }

    void Renderer::SetTime(float totalSeconds) { if (m_Impl) m_Impl->time = totalSeconds; }

    // ------------------------------------------------------------------ views
    RenderView& Renderer::GetMainView() { return *m_Main; }

    std::unique_ptr<RenderView> Renderer::CreateView(const std::string& name)
    {
        if (!m_Impl) return nullptr;
        std::unique_ptr<RenderView> view(new RenderView(this, name));
        m_Views.push_back(view.get());
        return view;
    }

    // ------------------------------------------------------------------ one picture
    void Renderer::Render(RenderView& view, Scene* scene, const std::shared_ptr<Camera>& camera)
    {
        if (!m_Impl || !camera) { Log("WARNING: Renderer::Render without renderer or camera\n"); return; }
        if (view.m_Owner != this) { Log("WARNING: Renderer::Render with a view of another renderer\n"); return; }

        // The renderer only READS the scene. World matrices are the application's to bring up to date
        // (TransformSystem::Update, after gameplay and animation, before Render); a stale one is drawn where it was.
#ifdef _DEBUG
        if (scene)
            assert(TransformSystem::IsCurrent(*scene) && "call TransformSystem::Update(scene) before drawing it");
#endif

        Impl& g = *m_Impl;
        FrameContext& f = g.frame;
        const bool firstView = (g.viewsThisFrame == 0);
        ++g.viewsThisFrame;
        if (firstView)       // one delta per FRAME, not per view
        {
            g.frameDelta = std::max(0.0f, g.time - g.lastFrameTime);
            g.lastFrameTime = g.time;
        }

        f.BeginFrame(g.frameDelta, g.time);
        f.camera = camera;
        f.cmd    = g.cmd.get();
        f.environment = g.environment;
        f.scene.world = scene;
        std::copy(view.GetClearColor(), view.GetClearColor() + 4, f.clearColor);

        // Ask EVERY frame: the device recreates the back buffer on resize and a view's texture changes with its panel,
        // a cached pointer would dangle.
        view.ApplySize(*g.services.device);
        if (view.DrawsToTexture())
        {
            f.targets.color = view.m_Color.get();
            f.targets.depth = view.m_Depth.get();
        }
        else
        {
            f.targets.color = g.services.device->GetBackBuffer();
            f.targets.depth = g.services.device->GetDepthBuffer();
        }

        g.cmd->Begin();
        if (view.DrawsToTexture() && firstView)     // the graph draws into the texture: the window itself is not touched by any pass
            g.cmd->ClearRenderTarget(g.services.device->GetBackBuffer(), kWindowClear[0], kWindowClear[1], kWindowClear[2], kWindowClear[3]);

        // ---- CPU systems: fill the blackboard
        if (f.scene.world)
        {
            ExtractionCamera ec;
            DirectX::XMStoreFloat4x4(&ec.view, f.camera->GetViewMatrix());
            DirectX::XMStoreFloat4x4(&ec.projection, f.camera->GetProjectionMatrix());
            ec.position = f.camera->GetPositionFloat3();
            g.extraction.Extract(*f.scene.world, ec, f.batches, f.stats);   // scene -> batches
            g.shadows.Update(f, g.extraction, ec);                          // which lights cast, where their tiles are, which casters
        }
        g.lights.Upload(f);                                                 // scene lights + environment -> lightBuffer

        // ---- GPU passes: read the blackboard
        GpuProfiler& gpu = *g.services.profiler;
        if (firstView) gpu.BeginFrame(*g.cmd);      // one timed picture per frame; scopes of the other views are no-ops
        g.graph.Execute(f);
        if (firstView) gpu.EndFrame(*g.cmd);

        g.cmd->End();
        g.services.device->Submit(g.cmd.get());

        view.m_Stats = f.stats;
        f.scene.Clear();             // drop the scene pointer now, not at the start of the next frame
        f.camera.reset();

        if (g.debugInfo && firstView) Log(GetDebugInfo().c_str());
    }

    // ------------------------------------------------------------------ the main view, in the old sequence
    void Renderer::BeginScene(const std::shared_ptr<Camera>& camera)
    {
        if (!m_Impl || !camera) { Log("WARNING: BeginScene without renderer or camera\n"); return; }
        m_Impl->pendingCamera = camera;
        m_Impl->pendingScene  = nullptr;
        m_Impl->inScene = true;
    }

    void Renderer::Submit(Scene& scene)
    {
        if (!m_Impl || !m_Impl->inScene) return;
        m_Impl->pendingScene = &scene;
    }

    void Renderer::EndScene()
    {
        if (!m_Impl || !m_Impl->inScene) return;
        Impl& g = *m_Impl;
        Scene* scene = g.pendingScene;
        std::shared_ptr<Camera> camera = std::move(g.pendingCamera);
        g.pendingScene = nullptr;
        g.pendingCamera.reset();
        g.inScene = false;
        Render(*m_Main, scene, camera);
    }

    void Renderer::SetClearColor(float r, float gr, float b, float a) { m_Main->SetClearColor(r, gr, b, a); }
    void Renderer::SetSceneTargetSize(int width, int height) { if (m_Impl) m_Main->SetSize(width, height); }
    RHI::ITexture* Renderer::GetSceneTexture() const { return m_Main->GetTexture(); }

    // ------------------------------------------------------------------ settings / queries
    void Renderer::OnWindowResize(int width, int height)
    {
        if (!m_Impl || width <= 0 || height <= 0) return;
        m_Impl->services.device->Resize(width, height);
        m_Impl->graph.OnResize(width, height);
    }

    void Renderer::Present()
    {
        if (!m_Impl) return;
        m_Impl->services.device->Present();
        m_Impl->viewsThisFrame = 0;
    }

    void Renderer::SetVSync(bool enabled)
    {
        if (m_Impl) m_Impl->services.device->SetVSync(enabled);
    }

    bool Renderer::IsVSyncEnabled() const
    {
        return m_Impl && m_Impl->services.device->IsVSync();
    }

    void Renderer::SetGpuProfilingEnabled(bool enabled)
    {
        if (m_Impl && m_Impl->services.profiler) m_Impl->services.profiler->SetEnabled(enabled);
    }

    bool Renderer::IsGpuProfilingEnabled() const
    {
        return m_Impl && m_Impl->services.profiler && m_Impl->services.profiler->IsEnabled();
    }

    bool Renderer::IsGpuProfilingAvailable() const
    {
        return m_Impl && m_Impl->services.profiler && m_Impl->services.profiler->IsAvailable();
    }

    const GpuFrameTimings& Renderer::GetGpuTimings() const
    {
        static const GpuFrameTimings kNone{};
        return (m_Impl && m_Impl->services.profiler) ? m_Impl->services.profiler->GetTimings() : kNone;
    }

    void Renderer::EnableWireframe(bool enable)
    {
        if (!m_Impl) return;
        m_Impl->wireframe = enable;
        m_Impl->services.resources->SetWireframeMode(enable);
    }

    bool Renderer::IsWireframeEnabled() const { return m_Impl && m_Impl->wireframe; }
    bool Renderer::IsFrustumCullingEnabled() const { return m_Impl && m_Impl->extraction.GetConfig().frustumCulling; }

    void Renderer::EnableFrustumCulling(bool enable)
    {
        if (!m_Impl) return;
        ExtractionConfig cfg = m_Impl->extraction.GetConfig();
        cfg.frustumCulling = enable;
        m_Impl->extraction.SetConfig(cfg);
    }

    void Renderer::EnableDebugInfo(bool enable) { if (m_Impl) m_Impl->debugInfo = enable; }

    const RenderStatistics& Renderer::GetStats() const { return m_Main->GetStats(); }

    std::string Renderer::GetDebugInfo() const
    {
        if (!m_Impl) return "Renderer not initialised\n";
        return m_Main->GetStats().ToString() + m_Impl->graph.GetDebugInfo() + m_Impl->services.resources->GetDebugInfo();
    }
}
