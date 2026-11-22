#pragma once
#include "RenderTypes.h"
#include "ShadowTypes.h"
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

namespace DXEngine
{
    class Scene;
    class Material;
    class Camera;
    class ResourceCache;
    class ShaderCache;
    class MaterialSystem;
    class GpuProfiler;

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
        std::shared_ptr<GpuProfiler>          profiler;    // GPU time per pass; null or unavailable = no timing
    };

    // ==================================================================
    // FrameContext: the blackboard for ONE frame.
    //
    //  WHO WRITES WHAT (this table is the contract):
    //   Renderer::Render            -> camera, cmd, targets, clear colour, scene, environment (one view at a time)
    //   SceneExtraction             -> batches (culling + material resolution + sorting)
    //   ShadowSystem                -> shadows (casters, views, atlas + buffers), shadow stats
    //   LightSystem                 -> lightBuffer, localLightBuffer, clusterBuffer, clusterRangeBuffer, lightIndexBuffer, stats
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
            void Clear() { world = nullptr; }
        } scene;

        RenderEnvironment environment;                            // ambient / exposure / gamma (copied in by Renderer)

        // ---- render targets: raw, NON-owning, re-fetched EVERY frame ----
        // (the device recreates the back buffer on resize, and the scene texture is replaced when its size changes,
        //  so caching either is a dangling-pointer bug)
        // 'color' is where the 3D scene and the UI are drawn: the window's back buffer, or the texture of the view being
        // drawn when it has one (RenderView::SetSize). Passes never need to know which.
        struct RenderTargets
        {
            RHI::ITexture* color = nullptr;
            RHI::ITexture* depth = nullptr;
        } targets;
        float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };         // the view's (Renderer::Render copies it in); ClearPass reads it

        // ---- produced by SceneExtraction ----
        BatchQueue batches;

        // ---- produced by ShadowSystem (BEFORE LightSystem: the light buffers point at the shadow views) ----
        ShadowFrame shadows;

        // ---- produced by LightSystem ----
        RHI::IBuffer* lightBuffer      = nullptr;   // constant buffer b3: environment + directional lights + local light count
        RHI::IBuffer* localLightBuffer = nullptr;   // StructuredBuffer<LightGPU> (point + spot lights), bound at SRV_Lights
        RHI::IBuffer* clusterBuffer      = nullptr; // constant buffer b7: the cluster grid (ClusterConstants)
        RHI::IBuffer* clusterRangeBuffer = nullptr; // StructuredBuffer<uint2>: {offset, count} per cluster, SRV_ClusterRanges
        RHI::IBuffer* lightIndexBuffer   = nullptr; // StructuredBuffer<uint>: the light indices of every cluster, SRV_LightIndices
        // Both are valid every frame once LightSystem is initialised (a scene without lights has a count of 0), and
        // both point into buffers that the LightSystem may REPLACE next frame when the scene needs more lights.

        // ---- produced by passes ----
        RenderStatistics stats;

        void BeginFrame(float dt, float total)
        {
            ++frameIndex;
            deltaTime = dt;
            totalTime = total;
            scene.Clear();
            batches.Clear();
            shadows.Clear();
            lightBuffer = nullptr;
            localLightBuffer = nullptr;
            clusterBuffer = nullptr;
            clusterRangeBuffer = nullptr;
            lightIndexBuffer = nullptr;
            stats.Reset();
        }
    };
}
