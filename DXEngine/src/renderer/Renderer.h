#pragma once
#include "Core/GpuTimings.h"
#include "Core/RenderTypes.h"
#include "Core/RenderView.h"
#include <memory>
#include <string>
#include <vector>

namespace DXEngine
{
    class Camera;
    class Scene;
    namespace RHI { class IGraphicsDevice; class ITexture; }

    // ==================================================================
    // Renderer: owns the device and every renderer service, and draws scenes through RenderViews. It contains no drawing
    // code of its own (that is the passes' and the systems'):
    //
    //   Render(view, scene, camera)   one complete picture: extract (cull, route, sort) -> shadows -> lights -> the render
    //                                 graph, drawn into the view's target
    //   Present()                     show the frame (Application calls it once per frame, after everything that draws on top)
    //
    // It is an INSTANCE, created and owned by the Application (and by anything else that needs a renderer: a tool, a
    // test with a device). Renderer::Get() is the PRIMARY one, the one most recently initialised, for the code that has no
    // way to be handed one (loaders that need the device); prefer passing the Renderer or the device.
    //
    // Views: the main view draws into the window's back buffer unless it was given a size. CreateView makes more (a second
    // viewport, a material preview); any number of views can draw the same scene, each with its own camera, one after the
    // other within a frame.
    //
    // The old BeginScene(camera) / Submit(scene) / EndScene() sequence is kept as the way to draw the MAIN view:
    // EndScene() is Render(GetMainView(), scene, camera).
    // ==================================================================
    class Renderer
    {
    public:
        Renderer();
        ~Renderer();                                        // Shutdown()
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // The primary renderer: the last one whose Init succeeded and whose Shutdown has not run.
        static bool HasInstance();
        static Renderer& Get();
        // The primary renderer's device, or null when there is none: for code that cannot be handed a device or a
        // renderer (loaders created without one). Safe to call at any time.
        static std::shared_ptr<RHI::IGraphicsDevice> PrimaryDevice();

        // Creates the device and every renderer service. False (with the reason sent to the debugger output) when that
        // failed; the renderer is then not initialised and every other call is a no-op.
        bool Init(void* windowHandle, int width, int height);
        void Shutdown();
        bool IsInitialized() const;

        // The GPU device, for code that creates GPU resources (meshes, textures) outside the renderer.
        // Null before Init / after Shutdown.
        std::shared_ptr<RHI::IGraphicsDevice> GetDevice() const;

        // Shading settings that are not lights (ambient, exposure, gamma, IBL strength): shared by every view.
        void SetEnvironment(const RenderEnvironment& environment);
        RenderEnvironment GetEnvironment() const;

        void SetTime(float totalSeconds);                   // Application passes time since start

        // ---- views ----
        RenderView& GetMainView();
        // A new view. Destroy it whenever you like (before or after the Renderer). Null before Init.
        std::unique_ptr<RenderView> CreateView(const std::string& name);

        // Draws 'scene' (may be null: only the clear) as seen by 'camera' into the view's target. The caller has run
        // TransformSystem::Update(scene) first (debug builds assert it). Views are drawn one after the other.
        // A view that draws into the back buffer clears ALL of it: at most one per frame, and no other view after it
        // may rely on the back buffer (views with a size draw into their own texture and do not touch it).
        void Render(RenderView& view, Scene* scene, const std::shared_ptr<Camera>& camera);

        // ---- the main view, in the old sequence ----
        void BeginScene(const std::shared_ptr<Camera>& camera);
        void Submit(Scene& scene);                          // read only: see Render
        void EndScene();                                    // = Render(GetMainView(), scene, camera)

        void SetClearColor(float r, float g, float b, float a = 1.0f);     // of the main view
        // Default: the main view draws straight into the window's back buffer. An editor wants the scene in a TEXTURE it
        // can show inside a panel: SetSceneTargetSize(w, h) with w, h > 0 turns that on, (0, 0) turns it off again.
        // (See RenderView::SetSize for when it is applied.) While the main view draws into a texture the renderer clears
        // the back buffer itself; whoever shows the texture draws over it before Present().
        void SetSceneTargetSize(int width, int height);
        RHI::ITexture* GetSceneTexture() const;             // the main view's texture, or null

        void OnWindowResize(int width, int height);

        // End of the frame: shows what was drawn.
        void Present();

        // Presentation interval: on waits for the display's vertical blank (no tearing, capped to the refresh rate), off
        // presents at once. Applied at the next Present().
        void SetVSync(bool enabled);
        bool IsVSyncEnabled() const;

        // GPU time per pass (timestamp queries, read back a few frames late so nothing ever waits). Available when the
        // device can create the queries; switching it off clears the numbers. Only the FIRST view drawn in a frame is
        // timed (the numbers are those of one picture, not a sum over views).
        void SetGpuProfilingEnabled(bool enabled);
        bool IsGpuProfilingEnabled() const;
        bool IsGpuProfilingAvailable() const;
        const GpuFrameTimings& GetGpuTimings() const;

        void EnableWireframe(bool enable);
        void EnableFrustumCulling(bool enable);
        bool IsWireframeEnabled() const;          // so a UI shows the real state, whoever changed it last
        bool IsFrustumCullingEnabled() const;
        void EnableDebugInfo(bool enable);

        const RenderStatistics& GetStats() const;           // of the main view's last Render
        std::string GetDebugInfo() const;

    private:
        friend class RenderView;
        void UnregisterView(RenderView* view);

        struct Impl;
        std::unique_ptr<Impl> m_Impl;
        std::vector<RenderView*> m_Views;                   // every live view, the main one included (non-owning)
        std::unique_ptr<RenderView> m_Main;
    };
}
