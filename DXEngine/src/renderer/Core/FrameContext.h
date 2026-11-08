#pragma once
#include "RenderTypes.h"
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <any>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace DXEngine
{
    class Scene;
    class Material;
    class Camera;
    class UIElement;
    class ResourceCache;
    class ShaderCache;
    class MaterialSystem;

    namespace RHI
    {
        class IGraphicsDevice;
        class ICommandBuffer;
        class ITexture;
        class IBuffer;
    }

    // ==================================================================
    // RenderServices: LONG-LIVED things. Owned by Renderer, created once.
    // FrameContext only POINTS at this. (Before, these shared_ptrs lived
    // inside FrameContext, mixing "per-frame data" with "engine services".)
    // ==================================================================
    struct RenderServices
    {
        std::shared_ptr<RHI::IGraphicsDevice> device;
        std::shared_ptr<ResourceCache>        resources;
        std::shared_ptr<ShaderCache>          shaders;
        std::shared_ptr<MaterialSystem>       materials;
    };

    // ==================================================================
    // FrameContext: the blackboard for ONE frame.
    //
    //  WHO WRITES WHAT (this table is the contract):
    //   Renderer::BeginScene/Submit -> camera, cmd, targets, scene, environment
    //   SceneExtraction             -> batches (culling + material resolution + sorting)
    //   LightSystem                 -> lightBuffer
    //   Passes                      -> stats (and the GPU targets, as side effects)
    //
    //  RULES
    //   1. Systems (CPU) fill the blackboard. Passes (GPU) read it.
    //   2. A field is added ONLY when something writes it. No "future" fields.
    //   3. Nothing keeps a pointer to the FrameContext or its raw pointers
    //      past the end of the frame.
    // ==================================================================
    class FrameContext
    {
    public:
        // ---- identity / time ----
        uint32_t frameIndex = 0;
        float    deltaTime  = 0.0f;   // seconds since previous frame
        float    totalTime  = 0.0f;   // seconds since application start

        // ---- long-lived services (non-owning) ----
        RenderServices* services = nullptr;

        // ---- inputs (written by Renderer) ----
        std::shared_ptr<Camera> camera;
        RHI::ICommandBuffer*    cmd = nullptr;

        struct SceneInput
        {
            Scene* world = nullptr;                               // the persistent 3D world (not owned)
            std::vector<std::shared_ptr<UIElement>> uiElements;   // 2D, rebuilt every frame
            void Clear() { world = nullptr; uiElements.clear(); }
        } scene;

        RenderEnvironment environment;                            // ambient / exposure / gamma (copied in by Renderer)

        // ---- render targets: raw, NON-owning, re-fetched from the device EVERY frame ----
        // (the device recreates them on resize, so caching them is a dangling-pointer bug)
        struct RenderTargets
        {
            RHI::ITexture* backBuffer  = nullptr;
            RHI::ITexture* depthBuffer = nullptr;
        } targets;

        // ---- produced by SceneExtraction ----
        BatchQueue batches;

        // ---- produced by LightSystem ----
        RHI::IBuffer* lightBuffer = nullptr;   // null when there are no lights

        // ---- produced by passes ----
        RenderStatistics stats;

        // ---- escape hatch for experiments ----
        // Typed fields above are preferred. Use this only for data that exactly one
        // pass writes and one pass reads (e.g. a future cluster grid).
        template<typename T>
        void Set(const std::string& key, T value) { m_Extra[key] = std::move(value); }

        // Returns nullptr when missing or of a different type. NEVER copies the data.
        template<typename T>
        const T* TryGet(const std::string& key) const
        {
            auto it = m_Extra.find(key);
            return it == m_Extra.end() ? nullptr : std::any_cast<T>(&it->second);
        }

        void BeginFrame(float dt, float total)
        {
            ++frameIndex;
            deltaTime = dt;
            totalTime = total;
            scene.Clear();
            batches.Clear();
            lightBuffer = nullptr;
            stats.Reset();
            m_Extra.clear();
        }

    private:
        std::unordered_map<std::string, std::any> m_Extra;
    };
}
