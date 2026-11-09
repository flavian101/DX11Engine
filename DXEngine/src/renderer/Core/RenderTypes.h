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

    struct RenderEnvironment
    {
        DirectX::XMFLOAT3 ambientColor{ 0.2f, 0.25f, 0.3f };
        float ambientIntensity = 0.1f;
        float iblIntensity     = 0.8f;
        float exposure         = 0.4f;
        float gamma            = 2.2f;
        LightCullingMode lightCulling = LightCullingMode::Clustered;
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
