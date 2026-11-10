#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace DXEngine
{
    class Mesh;
    class Material;

    // ------------------------------------------------------------------
    // DrawItem: ONE fully-resolved draw call.
    // The RenderBatcher builds these; passes only consume them.
    // Rule: a pass never touches Model, never resolves a material override,
    // never decides "is this transparent?". All of that is already decided here.
    // ------------------------------------------------------------------
    struct DrawItem
    {
        std::shared_ptr<Mesh>     mesh;
        std::shared_ptr<Material> material;      // already resolved: override ?? model's material
        uint32_t                  submeshIndex = 0;

        DirectX::XMFLOAT4X4 world;               // NOT transposed. The drawer transposes on upload.

        // Non-owning. Valid for the frame because FrameContext::scene keeps the Model alive.
        const std::vector<DirectX::XMFLOAT4X4>* instanceTransforms = nullptr;
        const std::vector<DirectX::XMFLOAT4X4>* boneMatrices       = nullptr;

        float distanceToCamera = 0.0f;           // used for sorting only
        bool  receivesShadows  = true;           // MeshRendererComponent::receivesShadows: skips the shadow lookup when false

        bool IsInstanced() const { return instanceTransforms && instanceTransforms->size() > 1; }
        bool IsSkinned()   const { return boneMatrices && !boneMatrices->empty(); }
    };

    // ------------------------------------------------------------------
    // BatchQueue: three lists, one per forward pass. (UI is not a DrawItem:
    // UI elements are not Models, UIPass reads FrameContext::scene.uiElements.)
    // ------------------------------------------------------------------
    struct BatchQueue
    {
        std::vector<DrawItem> background;   // skyboxes: drawn first, no sorting
        std::vector<DrawItem> opaque;       // sorted by state (material, mesh), then near-to-far
        std::vector<DrawItem> transparent;  // sorted far-to-near

        // clear() keeps vector capacity -> no per-frame heap allocation after warm-up
        void Clear() { background.clear(); opaque.clear(); transparent.clear(); }
    };

    // ------------------------------------------------------------------
    // RenderEnvironment: scene-wide shading settings that are not lights (what LightManager used to hold
    // next to the lights). Lights themselves are LightComponents in the Scene.
    // ------------------------------------------------------------------
    // How a pixel finds the lights that reach it.
    enum class LightCullingMode : uint32_t
    {
        None             = 0,   // every pixel loops over ALL local lights (the reference; slow with many lights)
        Clustered        = 1,   // every pixel loops over the lights of its cluster only (the default)
        ClusterHeatmap   = 2,   // clustered, and the screen shows lights per cluster: blue = few, red = many (debug view)
    };

    // ------------------------------------------------------------------
    // Shadow tuning. Lives in RenderEnvironment so a game (or the sandbox) can change it every frame; the planner
    // reads a copy, nothing caches it. Resolutions are rounded DOWN to a power of two (the atlas allocator needs it).
    // ------------------------------------------------------------------
    struct ShadowSettings
    {
        bool     enabled = true;

        // ---- the atlas: ONE depth texture holds every shadow view as a tile ----
        uint32_t atlasSize         = 4096;   // 4096^2 * 4 bytes = 64 MB. 2048 = 16 MB for weak GPUs.
        uint32_t minTileResolution = 128;    // a view is never squeezed below this; it is dropped instead

        // ---- directional light: cascaded shadow maps ----
        uint32_t cascadeCount        = 4;       // 1..4
        uint32_t cascadeResolution   = 1024;    // one tile per cascade
        float    maxDistance         = 100.0f;  // shadows end here (view-space depth); capped by the camera far plane
        float    cascadeLambda       = 0.75f;   // 0 = even splits, 1 = logarithmic splits
        float    fadeFraction        = 0.10f;   // the last 10% of the last cascade fade out instead of popping
        uint32_t maxShadowedDirectional = 1;    // every directional light with shadows costs cascadeCount tiles

        // ---- local lights: the most important ones get shadows, the rest stay unshadowed ----
        uint32_t maxShadowedLocalLights = 8;
        uint32_t spotResolution         = 1024; // the biggest tile a spot light gets (smaller when far away)
        uint32_t pointFaceResolution    = 512;  // ... and a point light's six faces

        // ---- quality / bias (all in TEXELS, so they stay right when the resolution changes) ----
        uint32_t pcfRadius       = 1;      // 0 = hard edge, 1 = 3x3 taps, 2 = 5x5, 3 = 7x7 (each tap is itself a 2x2 PCF)
        float    normalBias      = 1.5f;   // receiver is pushed this many texels along its normal before the lookup
        float    constantBias    = 2.0f;   // extra depth bias in texels (directional); scaled for perspective views
        int32_t  rasterDepthBias = 100;    // D3D11 DepthBias of the shadow pass (depth-buffer units)
        float    rasterSlopeBias = 2.0f;   // D3D11 SlopeScaledDepthBias of the shadow pass
        float    casterExtent    = 150.0f; // how far TOWARD the light a directional view still sees casters

        bool     snapToTexels = true;      // round cascade positions to whole texels: no shimmering when the camera moves
    };

    struct RenderEnvironment
    {
        DirectX::XMFLOAT3 ambientColor{ 0.2f, 0.25f, 0.3f };
        float ambientIntensity = 0.1f;
        float iblIntensity     = 0.8f;
        float exposure         = 0.4f;
        float gamma            = 2.2f;
        LightCullingMode lightCulling = LightCullingMode::Clustered;
        ShadowSettings   shadows;
    };

    // One statistics struct for the whole renderer (replaces 3 overlapping ones).
    struct RenderStatistics
    {
        uint32_t objectsSubmitted   = 0;
        uint32_t objectsCulled      = 0;
        uint32_t drawCalls          = 0;
        uint32_t trianglesRendered  = 0;
        uint32_t pipelineChanges    = 0;
        uint32_t materialChanges    = 0;
        uint32_t uiElementsRendered = 0;
        uint32_t directionalLights  = 0;   // written by LightSystem
        uint32_t localLights        = 0;   // point + spot lights uploaded this frame
        uint32_t lightsDropped      = 0;   // local lights over the limit (0 in a healthy scene)
        uint32_t clusterAssignments = 0;   // (light, cluster) pairs this frame; 0 when clustering is off
        uint32_t clusterMaxLights   = 0;   // the most lights any cluster holds = the worst a pixel loops over
        uint32_t clusterDropped     = 0;   // pairs lost because the cluster index list was full (0 in a healthy scene)
        uint32_t shadowedDirectional = 0;  // directional lights that got cascades              (written by ShadowSystem)
        uint32_t shadowedLocal       = 0;  // point + spot lights that got shadow tiles
        uint32_t shadowLightsDropped = 0;  // lights that wanted shadows but were over budget / did not fit the atlas
        uint32_t shadowViews         = 0;  // depth renders this frame (cascades + spot views + point faces)
        uint32_t shadowCasterDraws   = 0;  // draw calls of the shadow pass                    (written by ShadowPass)

        void Reset() { *this = RenderStatistics{}; }

        std::string ToString() const
        {
            std::ostringstream oss;
            oss << "Renderer stats:\n"
                << "  submitted " << objectsSubmitted << ", culled " << objectsCulled << "\n"
                << "  draw calls " << drawCalls << ", triangles " << trianglesRendered << "\n"
                << "  pipeline changes " << pipelineChanges
                << ", material changes " << materialChanges << "\n"
                << "  ui elements " << uiElementsRendered << "\n"
                << "  lights: " << directionalLights << " directional, " << localLights << " local";
            if (lightsDropped) oss << " (" << lightsDropped << " DROPPED: over the limit)";
            oss << "\n";
            if (shadowViews || shadowLightsDropped)
            {
                oss << "  shadows: " << shadowedDirectional << " directional, " << shadowedLocal << " local, "
                    << shadowViews << " views, " << shadowCasterDraws << " caster draws";
                if (shadowLightsDropped) oss << " (" << shadowLightsDropped << " lights DROPPED: over budget / atlas full)";
                oss << "\n";
            }
            if (clusterAssignments)
            {
                oss << "  clusters: " << clusterAssignments << " light assignments, at most " << clusterMaxLights << " lights per cluster";
                if (clusterDropped) oss << " (" << clusterDropped << " DROPPED: cluster index list full)";
                oss << "\n";
            }
            return oss.str();
        }
    };
}
