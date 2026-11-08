#pragma once
#include "Core/RenderTypes.h"
#include <memory>
#include <string>

namespace DXEngine
{
    class Camera;
    class Scene;
    class UIElement;
    namespace RHI { class IGraphicsDevice; }

    // ==================================================================
    // Renderer: the FACADE. It contains no drawing code:
    //
    //   BeginScene(camera)     start a frame: fill FrameContext (camera, targets, command buffer)
    //   Submit(scene)          the 3D world to draw (the Scene is persistent; nothing is re-submitted per object)
    //   SubmitUI(element)      2D UI for this frame
    //   EndScene()             extract (cull, route, sort) -> pack lights -> run the render graph -> present
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

        static void EnableWireframe(bool enable);
        static void EnableFrustumCulling(bool enable);
        static void EnableDebugInfo(bool enable);

        static const RenderStatistics& GetStats();
        static std::string GetDebugInfo();
    };
}
