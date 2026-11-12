#include "dxpch.h"
#include "Renderer.h"

#include "Core/FrameContext.h"
#include "Core/RenderGraph.h"
#include "Core/SceneTarget.h"
#include "Cache/ResourceCache.h"
#include "MaterialSystem.h"
#include "ShaderCache.h"
#include "Passes/ClearPass.h"
#include "Passes/ForwardPass.h"
#include "Passes/ShadowPass.h"
#include "Passes/UIPass.h"
#include "Systems/LightSystem.h"
#include "Systems/SceneExtraction.h"
#include "Systems/ShadowSystem.h"

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
            ShadowSystem    shadows;                         // BEFORE lights: the light buffers point at its views
            LightSystem     lights;
            RenderEnvironment environment;                   // ambient / exposure / gamma

            // The editor's scene texture (null = the scene is drawn into the back buffer). 'request' is what was asked
            // for, 'size' what exists; BeginScene makes the second follow the first.
            std::shared_ptr<RHI::ITexture> sceneColor;
            std::shared_ptr<RHI::ITexture> sceneDepth;
            int             requestWidth  = 0;
            int             requestHeight = 0;
            SceneTargetSize sceneSize;

            RenderStatistics lastStats;
            float time = 0.0f;
            bool  inScene = false;
            bool  debugInfo = false;
            bool  wireframe = false;
        };

        std::unique_ptr<State> g;
        const RenderStatistics kEmptyStats{};

        void Log(const char* msg) { OutputDebugStringA(msg); }

        // The colour the window is cleared to while the scene is drawn into a texture (nothing in the graph touches
        // the back buffer then). The editor covers all of it with its panels; this is what shows between frames of a resize.
        constexpr float kWindowClear[4] = { 0.09f, 0.09f, 0.10f, 1.0f };

        // Makes the scene texture match the requested size. Called at the start of a frame.
        void UpdateSceneTarget()
        {
            RenderServices& s = g->services;
            const SceneTargetSize want = ResolveSceneTargetSize(g->requestWidth, g->requestHeight,
                                                                s.device->GetCapabilities().maxTextureSize);
            if (want == g->sceneSize && (g->sceneColor != nullptr) == want.Enabled())
                return;

            g->sceneColor.reset();
            g->sceneDepth.reset();
            g->sceneSize = {};
            if (!want.Enabled())
                return;

            RHI::TextureDesc color;
            color.width  = want.width;
            color.height = want.height;
            color.format = RHI::TextureFormat::RGBA8_UNORM;        // the same as the back buffer: pipelines do not care which one they draw to
            color.isRenderTarget = true;
            color.debugName = "SceneColor";

            RHI::TextureDesc depth = color;
            depth.format = RHI::TextureFormat::D24_UNORM_S8_UINT;  // the same as the device's depth buffer
            depth.isRenderTarget = false;
            depth.isDepthStencil = true;
            depth.debugName = "SceneDepth";

            try
            {
                g->sceneColor = s.device->CreateTexture(color);
                g->sceneDepth = s.device->CreateTexture(depth);
                g->sceneSize  = want;
            }
            catch (const std::exception& e)       // D3D11Texture reports failure by throwing
            {
                Log((std::string("WARNING: scene texture could not be created, drawing into the back buffer: ") + e.what() + "\n").c_str());
                g->sceneColor.reset();
                g->sceneDepth.reset();
                g->requestWidth = g->requestHeight = 0;   // do not retry (and log) every frame
            }
        }
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
            !g->shadows.Initialize(s) || !g->lights.Initialize(s))
        {
            Log("ERROR: Renderer::Init - service initialisation failed\n");
            g.reset();
            return;
        }

        g->cmd = s.device->CreateCommandBuffer();
        g->frame.services = &g->services;

        // 3) the frame: passes run in the order they are added
        g->graph.AddPass<ShadowPass>("shadow");          // first: every lit draw below samples the atlas it fills
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
        g->sceneColor.reset();
        g->sceneDepth.reset();
        g->services.resources->Clear();
        g->cmd.reset();
        g->services.materials.reset();
        g->services.shaders.reset();
        g->services.resources.reset();
        g->services.device->Shutdown();     // device last: everything above may hold GPU objects
        g->services.device.reset();
        g.reset();
    }

    std::shared_ptr<RHI::IGraphicsDevice> Renderer::GetDevice()
    {
        return g ? g->services.device : std::shared_ptr<RHI::IGraphicsDevice>();
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

        // Ask EVERY frame: the device recreates the back buffer on resize and the scene texture changes with its panel,
        // a cached pointer would dangle.
        UpdateSceneTarget();
        if (g->sceneColor)
        {
            f.targets.color = g->sceneColor.get();
            f.targets.depth = g->sceneDepth.get();
        }
        else
        {
            f.targets.color = g->services.device->GetBackBuffer();
            f.targets.depth = g->services.device->GetDepthBuffer();
        }

        g->cmd->Begin();
        if (g->sceneColor)      // the graph draws into the texture: the window itself is not touched by any pass
            g->cmd->ClearRenderTarget(g->services.device->GetBackBuffer(), kWindowClear[0], kWindowClear[1], kWindowClear[2], kWindowClear[3]);
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
            g->shadows.Update(f, g->extraction, camera);                          // which lights cast, where their tiles are, which casters
        }
        g->lights.Upload(f);                                                    // scene lights + environment -> lightBuffer

        // ---- GPU passes: read the blackboard
        g->graph.Execute(f);

        g->cmd->End();
        g->services.device->Submit(g->cmd.get());

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

    void Renderer::SetSceneTargetSize(int width, int height)
    {
        if (!g) return;
        g->requestWidth  = width;
        g->requestHeight = height;       // applied by the next BeginScene
    }

    RHI::ITexture* Renderer::GetSceneTexture() { return g ? g->sceneColor.get() : nullptr; }

    void Renderer::Present()
    {
        if (g) g->services.device->Present();
    }

    void Renderer::EnableWireframe(bool enable)
    {
        if (!g) return;
        g->wireframe = enable;
        g->services.resources->SetWireframeMode(enable);
    }

    bool Renderer::IsWireframeEnabled() { return g && g->wireframe; }
    bool Renderer::IsFrustumCullingEnabled() { return g && g->extraction.GetConfig().frustumCulling; }

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
