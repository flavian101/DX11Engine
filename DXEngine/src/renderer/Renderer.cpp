#include "dxpch.h"
#include "Renderer.h"

#include "Core/FrameContext.h"
#include "Core/RenderGraph.h"
#include "Cache/ResourceCache.h"
#include "MaterialSystem.h"
#include "ShaderCache.h"
#include "Passes/ClearPass.h"
#include "Passes/ForwardPass.h"
#include "Passes/UIPass.h"
#include "Systems/LightSystem.h"
#include "Systems/SceneExtraction.h"

#include "RHI/GraphicsDevice.h"
#include "camera/Camera.h"
#include "scene/Scene.h"
#include "scene/TransformSystem.h"
#include "utils/UI/UIElement.h"
#include <algorithm>

namespace DXEngine
{
    namespace
    {
        // Everything the renderer owns, in ONE place. Created by Init, destroyed by Shutdown.
        struct State
        {
            RenderServices services;                         // long-lived
            std::shared_ptr<RHI::ICommandBuffer> cmd;

            FrameContext frame;                              // the blackboard (reused every frame)
            RenderGraph  graph;                              // GPU passes
            std::shared_ptr<ClearPass> clearPass;            // typed handle for SetClearColor

            SceneExtraction extraction;                      // CPU systems, run by EndScene
            LightSystem     lights;
            RenderEnvironment environment;                   // ambient / exposure / gamma

            RenderStatistics lastStats;
            float time = 0.0f;
            bool  inScene = false;
            bool  debugInfo = false;
        };

        std::unique_ptr<State> g;
        const RenderStatistics kEmptyStats{};

        void Log(const char* msg) { OutputDebugStringA(msg); }
    }

    // ------------------------------------------------------------------ lifetime
    void Renderer::Init(void* windowHandle, int width, int height)
    {
        g = std::make_unique<State>();
        RenderServices& s = g->services;

        // 1) device
        s.device = std::shared_ptr<RHI::IGraphicsDevice>(
            RHI::GraphicsDeviceFactory::Create(RHI::GraphicsAPI::DirectX11));
        if (!s.device || !s.device->Initialize(windowHandle, width, height))
        {
            Log("ERROR: Renderer::Init - device creation failed\n");
            g.reset();
            return;
        }

        // 2) services (order matters: shaders and materials need the device; materials need shaders)
        s.resources = std::make_shared<ResourceCache>(s.device);
        s.shaders   = std::make_shared<ShaderCache>(s.device);
        s.materials = std::make_shared<MaterialSystem>(s.device, s.shaders);
        if (!s.resources->Initialize() || !s.shaders->Initialize() || !s.materials->Initialize() ||
            !g->lights.Initialize(s))
        {
            Log("ERROR: Renderer::Init - service initialisation failed\n");
            g.reset();
            return;
        }

        g->cmd = s.device->CreateCommandBuffer();
        g->frame.services = &g->services;

        // 3) the frame: passes run in the order they are added
        g->clearPass = g->graph.AddPass<ClearPass>("clear");
        g->graph.AddPass<ForwardPass>("background",  ForwardPass::Queue::Background);
        g->graph.AddPass<ForwardPass>("opaque",      ForwardPass::Queue::Opaque);
        g->graph.AddPass<ForwardPass>("transparent", ForwardPass::Queue::Transparent);
        g->graph.AddPass<UIPass>("ui");

        if (!g->graph.Compile() || !g->graph.Initialize(g->frame))
        {
            Log("ERROR: Renderer::Init - render graph setup failed\n");
            g.reset();
            return;
        }
        Log("Renderer initialised\n");
    }

    void Renderer::Shutdown()
    {
        if (!g) return;
        g->graph.Shutdown();
        g->services.resources->Clear();
        g->cmd.reset();
        g->services.materials.reset();
        g->services.shaders.reset();
        g->services.resources.reset();
        g->services.device->Shutdown();     // device last: everything above may hold GPU objects
        g->services.device.reset();
        g.reset();
    }

    void Renderer::SetEnvironment(const RenderEnvironment& environment)
    {
        if (g) g->environment = environment;
    }

    RenderEnvironment Renderer::GetEnvironment()
    {
        return g ? g->environment : RenderEnvironment();
    }

    // ------------------------------------------------------------------ per frame
    void Renderer::SetTime(float totalSeconds) { if (g) g->time = totalSeconds; }

    void Renderer::SetClearColor(float r, float gr, float b, float a)
    {
        if (g && g->clearPass) g->clearPass->SetColor(r, gr, b, a);
    }

    void Renderer::BeginScene(const std::shared_ptr<Camera>& camera)
    {
        if (!g || !camera) { Log("WARNING: BeginScene without renderer or camera\n"); return; }

        FrameContext& f = g->frame;
        const float delta = std::max(0.0f, g->time - f.totalTime);    // before BeginFrame overwrites totalTime
        f.BeginFrame(delta, g->time);

        f.camera = camera;
        f.cmd    = g->cmd.get();
        f.environment = g->environment;

        // Ask the device EVERY frame: it recreates these on resize, a cached pointer would dangle.
        f.targets.backBuffer  = g->services.device->GetBackBuffer();
        f.targets.depthBuffer = g->services.device->GetDepthBuffer();

        g->cmd->Begin();
        g->inScene = true;
    }

    void Renderer::Submit(Scene& scene)
    {
        if (!g || !g->inScene) return;
        TransformSystem::Update(scene);          // world matrices are derived data: make sure they are current
        g->frame.scene.world = &scene;
    }

    void Renderer::SubmitUI(const std::shared_ptr<UIElement>& element)
    {
        if (g && g->inScene && element) g->frame.scene.uiElements.push_back(element);
    }

    void Renderer::EndScene()
    {
        if (!g || !g->inScene) return;
        FrameContext& f = g->frame;

        // ---- CPU systems: fill the blackboard
        if (f.scene.world && f.camera)
        {
            ExtractionCamera camera;
            DirectX::XMStoreFloat4x4(&camera.view, f.camera->GetViewMatrix());
            DirectX::XMStoreFloat4x4(&camera.projection, f.camera->GetProjectionMatrix());
            camera.position = f.camera->GetPositionFloat3();
            g->extraction.Extract(*f.scene.world, camera, f.batches, f.stats);   // scene -> batches
        }
        g->lights.Upload(f);                                                    // scene lights + environment -> lightBuffer

        // ---- GPU passes: read the blackboard
        g->graph.Execute(f);

        g->cmd->End();
        g->services.device->Submit(g->cmd.get());
        g->services.device->Present();

        g->lastStats = f.stats;
        f.scene.Clear();             // release models/UI now, not at the start of the next frame
        f.camera.reset();
        g->inScene = false;

        if (g->debugInfo) Log(GetDebugInfo().c_str());
    }

    // ------------------------------------------------------------------ settings / queries
    void Renderer::OnWindowResize(int width, int height)
    {
        if (!g || width <= 0 || height <= 0) return;
        g->services.device->Resize(width, height);
        g->graph.OnResize(width, height);
    }

    void Renderer::EnableWireframe(bool enable)
    {
        if (g) g->services.resources->SetWireframeMode(enable);
    }

    void Renderer::EnableFrustumCulling(bool enable)
    {
        if (!g) return;
        ExtractionConfig cfg = g->extraction.GetConfig();
        cfg.frustumCulling = enable;
        g->extraction.SetConfig(cfg);
    }

    void Renderer::EnableDebugInfo(bool enable) { if (g) g->debugInfo = enable; }

    const RenderStatistics& Renderer::GetStats() { return g ? g->lastStats : kEmptyStats; }

    std::string Renderer::GetDebugInfo()
    {
        if (!g) return "Renderer not initialised\n";
        return g->lastStats.ToString() + g->graph.GetDebugInfo() + g->services.resources->GetDebugInfo();
    }
}
