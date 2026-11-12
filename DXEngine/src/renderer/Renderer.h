#pragma once
#include "Core/RenderTypes.h"
#include <memory>
#include <string>

namespace DXEngine
{
    class Camera;
    class Scene;
    class UIElement;
    namespace RHI { class IGraphicsDevice; class ITexture; }

    // ==================================================================
    // Renderer: the FACADE. It contains no drawing code:
    //
    //   BeginScene(camera)     start a frame: fill FrameContext (camera, targets, command buffer)
    //   Submit(scene)          the 3D world to draw (the Scene is persistent; nothing is re-submitted per object)
    //   SubmitUI(element)      2D UI for this frame
    //   EndScene()             extract (cull, route, sort) -> pack lights -> run the render graph
    //   Present()              show the frame (Application calls it once per frame, after everything that draws on top)
    // ==================================================================
    class Renderer
    {
    public:
        static void Init(void* windowHandle, int width, int height);
        static void Shutdown();

        // The GPU device, for code that creates GPU resources (meshes, textures) outside the renderer.
        // Null before Init / after Shutdown.
        static std::shared_ptr<RHI::IGraphicsDevice> GetDevice();

        // Scene-wide shading settings that are not lights (ambient, exposure, gamma, IBL strength).
        static void SetEnvironment(const RenderEnvironment& environment);
        static RenderEnvironment GetEnvironment();

        static void SetTime(float totalSeconds);          // Application passes time since start
        static void SetClearColor(float r, float g, float b, float a = 1.0f);

        static void BeginScene(const std::shared_ptr<Camera>& camera);
        // Updates the scene's world matrices (TransformSystem) and remembers it for EndScene.
        static void Submit(Scene& scene);
        static void SubmitUI(const std::shared_ptr<UIElement>& element);
        static void EndScene();

        static void OnWindowResize(int width, int height);

        // ---- where the scene is drawn ----
        // Default: straight into the window's back buffer. An editor wants the scene in a TEXTURE it can show inside a
        // panel: SetSceneTargetSize(w, h) with w, h > 0 turns that on, (0, 0) turns it off again.
        //  * The new size is applied at the next BeginScene, never in the middle of a frame (the UI being built may still
        //    point at the old texture).
        //  * While it is on the renderer clears the back buffer itself; whoever shows the texture draws over it before Present().
        //  * Sizes are clamped to what the device supports (see ResolveSceneTargetSize).
        static void SetSceneTargetSize(int width, int height);
        // The texture the scene is drawn into, or null while it is drawn into the back buffer. It is replaced when the
        // size changes, so do not keep the pointer past the frame.
        static RHI::ITexture* GetSceneTexture();

        // End of the frame: shows what was drawn. (EndScene used to do this; it moved so that something can still be drawn
        // on top of the finished scene, like the ImGui layer.)
        static void Present();

        static void EnableWireframe(bool enable);
        static void EnableFrustumCulling(bool enable);
        static bool IsWireframeEnabled();         // so a UI shows the real state, whoever changed it last
        static bool IsFrustumCullingEnabled();
        static void EnableDebugInfo(bool enable);

        static const RenderStatistics& GetStats();
        static std::string GetDebugInfo();
    };
}
