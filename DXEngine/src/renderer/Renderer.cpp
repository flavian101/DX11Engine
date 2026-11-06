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
#include "Systems/CullingSystem.h"
#include "Systems/LightSystem.h"
#include "Systems/RenderBatcher.h"

#include "RHI/GraphicsDevice.h"
#include "camera/Camera.h"
#include "models/Model.h"
#include "utils/Light.h"
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

            CullingSystem culling;                           // CPU systems, run by EndScene
            RenderBatcher batcher;
            LightSystem   lights;

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
        g->services.lights.reset();
        g->services.materials.reset();
        g->services.shaders.reset();
        g->services.resources.reset();
        g->services.device->Shutdown();     // device last: everything above may hold GPU objects
        g->services.device.reset();
        g.reset();
    }

    void Renderer::InitLightManager()
    {
        if (!g) return;
        auto lm = std::make_shared<LightManager>();

        lm->SetAmbientLight({ 0.2f, 0.25f, 0.3f }, 0.1f);
        lm->SetExposure(0.4f);
        lm->SetGamma(2.2f);
        lm->SetIBLIntensity(0.8f);

        // No default point light: lights belong to the scene (the Sandbox's LightSphere creates and moves its
        // own). The old default sat at a fixed (0, 20, 4) with radius 20, so nothing at floor level was lit.
        g->services.lights = lm;
    }

    std::shared_ptr<RHI::IGraphicsDevice> Renderer::GetDevice()
    {
        return g ? g->services.device : nullptr;
    }

    std::shared_ptr<LightManager> Renderer::GetLightManager()
    {
        return g ? g->services.lights : nullptr;
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

        // Ask the device EVERY frame: it recreates these on resize, a cached pointer would dangle.
        f.targets.backBuffer  = g->services.device->GetBackBuffer();
        f.targets.depthBuffer = g->services.device->GetDepthBuffer();

        g->cmd->Begin();
        g->inScene = true;
    }

    void Renderer::Submit(const std::shared_ptr<Model>& model)
    {
        if (g && g->inScene && model) g->frame.scene.models.push_back({ model, nullptr });
    }

    void Renderer::Submit(const std::shared_ptr<Model>& model, const std::shared_ptr<Material>& materialOverride)
    {
        if (g && g->inScene && model) g->frame.scene.models.push_back({ model, materialOverride });
    }

    void Renderer::SubmitUI(const std::shared_ptr<UIElement>& element)
    {
        if (g && g->inScene && element) g->frame.scene.uiElements.push_back(element);
    }

    void Renderer::EndScene()
    {
        if (!g || !g->inScene) return;
        FrameContext& f = g->frame;

        // ---- CPU systems: fill the blackboard (order matters, each reads the previous one's output)
        g->culling.Cull(f);          // scene       -> visibility
        g->batcher.BuildBatches(f);  // visibility  -> batches
        g->lights.Upload(f);         // frustum     -> lightBuffer

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

    void Renderer::UpdateUIProjectionMatrix(int, int) {}   // UIPass derives it from the back buffer each frame

    void Renderer::EnableWireframe(bool enable)
    {
        if (g) g->services.resources->SetWireframeMode(enable);
    }

    void Renderer::EnableFrustumCulling(bool enable)
    {
        if (!g) return;
        CullingConfig cfg = g->culling.GetConfig();
        cfg.enableFrustumCulling = enable;
        g->culling.SetConfig(cfg);
    }

    void Renderer::EnableDebugInfo(bool enable) { if (g) g->debugInfo = enable; }

    const RenderStatistics& Renderer::GetStats() { return g ? g->lastStats : kEmptyStats; }

    std::string Renderer::GetDebugInfo()
    {
        if (!g) return "Renderer not initialised\n";
        return g->lastStats.ToString() + g->graph.GetDebugInfo() + g->services.resources->GetDebugInfo();
    }
}
