#pragma once
#include "../Core/RenderTypes.h"
#include <DirectXCollision.h>
#include <DirectXMath.h>

namespace DXEngine
{
    class Scene;

    // What extraction needs to know about the camera (plain data: no Camera class, so it is testable).
    struct ExtractionCamera
    {
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
        DirectX::XMFLOAT3   position;
    };

    struct ExtractionConfig
    {
        bool  frustumCulling = true;
        float boundsMargin   = 1.05f;   // safety margin on bounding spheres
    };

    // ==================================================================
    // SceneExtraction: Scene -> BatchQueue. Replaces CullingSystem + RenderBatcher, which worked on a flat list of
    // Models submitted every frame. This reads the persistent Scene:
    //
    //   for every entity with Transform + MeshRenderer:
    //     skip if hidden / mesh invalid,  frustum-cull by world bounding sphere,
    //     resolve one material per submesh, route to background / opaque / transparent,
    //     sort (opaque by material then mesh then near-to-far; transparent far-to-near).
    //
    // Requires TransformSystem::Update to have run this frame (it uses TransformComponent::world).
    // ==================================================================
    class SceneExtraction
    {
    public:
        void SetConfig(const ExtractionConfig& config) { m_Config = config; }
        const ExtractionConfig& GetConfig() const { return m_Config; }

        void Extract(Scene& scene, const ExtractionCamera& camera, BatchQueue& out, RenderStatistics& stats) const;

        // Every opaque mesh that casts a shadow, with its world bounding sphere (items[i] <-> bounds[i], margin included).
        // NOT culled against the camera: an object behind the camera or outside the view still throws a shadow into it, so
        // the shadow planner culls these per light view instead. Transparent materials and the sky never cast.
        // Both vectors are cleared first (capacity kept) and sorted by mesh so the shadow pass binds each mesh once.
        void ExtractShadowCasters(Scene& scene, std::vector<DrawItem>& items, std::vector<DirectX::BoundingSphere>& bounds) const;

        static DirectX::BoundingFrustum BuildFrustum(const ExtractionCamera& camera);

    private:
        ExtractionConfig m_Config;
    };
}
