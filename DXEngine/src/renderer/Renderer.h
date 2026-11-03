#pragma once
#include "Core/RenderTypes.h"
#include <memory>
#include <string>

namespace DXEngine
{
    class Camera;
    class Model;
    class Material;
    class UIElement;
    class LightManager;

    // ==================================================================
    // Renderer: the FACADE. Static API kept identical to the legacy renderer so Application,
    // Sandbox and UIManager keep compiling. It contains NO drawing code:
    //
    //   BeginScene -> fill FrameContext (camera, targets, cmd)
    //   Submit*    -> append to FrameContext::scene
    //   EndScene   -> Cull -> Batch -> UploadLights -> graph.Execute -> Present
    //
    // Everything else lives in systems (CPU) and passes (GPU).
    // ==================================================================
    class Renderer
    {
    public:
        static void Init(void* windowHandle, int width, int height);
        static void Shutdown();

        static void InitLightManager();
        static std::shared_ptr<LightManager> GetLightManager();

        static void SetTime(float totalSeconds);          // Application passes time since start
        static void SetClearColor(float r, float g, float b, float a = 1.0f);

        static void BeginScene(const std::shared_ptr<Camera>& camera);
        static void EndScene();

        static void Submit(const std::shared_ptr<Model>& model);
        static void Submit(const std::shared_ptr<Model>& model, const std::shared_ptr<Material>& materialOverride);
        static void SubmitUI(const std::shared_ptr<UIElement>& element);

        static void OnWindowResize(int width, int height);
        static void UpdateUIProjectionMatrix(int width, int height);   // deprecated no-op, see UIPass

        static void EnableWireframe(bool enable);
        static void EnableFrustumCulling(bool enable);
        static void EnableDebugInfo(bool enable);

        static const RenderStatistics& GetStats();
        static std::string GetDebugInfo();
    };
}
