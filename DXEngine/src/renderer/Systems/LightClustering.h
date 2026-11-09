#pragma once
#include "utils/ConstantBufferTypes.h"   // LightGPU, ClusterConstants
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // Clustered light assignment (pure CPU logic, no GPU calls: unit-tested).
    //
    // The view frustum is cut into a grid of CLUSTERS: tilesX x tilesY screen tiles, each split into slicesZ depth
    // slices. Slices grow exponentially with distance (near = thin, far = thick), so a cluster is roughly as wide as
    // it is deep at every distance. Every light is assigned to the clusters its sphere of influence touches; a pixel
    // then only has to look at the lights of ITS cluster instead of all of them.
    //
    // Output (what the GPU reads):
    //   Ranges()   one {offset, count} per cluster
    //   Indices()  a flat list of light indices; cluster c owns Indices()[range.offset .. offset + count)
    //
    // Conventions: left-handed view space (+Z forward), row-vector matrices (DirectXMath), a perspective projection
    // built with XMMatrixPerspectiveFovLH (a centred or off-centre one; no reverse-Z, no orthographic).
    // ==================================================================
    struct ClusterGridDesc
    {
        uint32_t tilesX = 16;
        uint32_t tilesY = 9;
        uint32_t slicesZ = 24;

        // Capacity of the index list (4 bytes each: 1048576 = 4 MB of GPU memory; only the used part is uploaded).
        // A few hundred lights with a large range can need hundreds of thousands of entries: every cluster a light
        // reaches costs one. When the lights need more, the clusters furthest down the list (far slices) lose
        // lights first and the loss is reported in ClusterStats::assignmentsDropped.
        uint32_t maxLightIndices = 1u << 20;
    };

    struct ClusterRange
    {
        uint32_t offset;
        uint32_t count;
    };
    static_assert(sizeof(ClusterRange) == 8, "ClusterRange is read as StructuredBuffer<uint2>");

    struct ClusterAabb
    {
        DirectX::XMFLOAT3 min;
        DirectX::XMFLOAT3 max;
    };

    struct ClusterStats
    {
        uint32_t lightsInFrustum       = 0;   // lights that touch at least one cluster
        uint32_t lightsCulled          = 0;   // lights that touch none (behind the camera, off screen, beyond far)
        uint32_t assignments           = 0;   // (light, cluster) pairs wanted
        uint32_t assignmentsDropped    = 0;   // pairs lost because the index list was full (0 in a healthy scene)
        uint32_t maxLightsPerCluster   = 0;   // after truncation: the most lights any pixel will loop over
        uint32_t nonEmptyClusters      = 0;
    };

    // ---- projection helpers (perspective, LH, row-vector) -----------------------------------------------------
    bool  ProjectionIsPerspective(const DirectX::XMFLOAT4X4& projection);
    float ProjectionNearPlane(const DirectX::XMFLOAT4X4& projection);
    float ProjectionFarPlane(const DirectX::XMFLOAT4X4& projection);

    // Closed-interval sphere / box overlap (touching counts). Exposed because it is the definition of "this light
    // reaches this cluster", and the tests compare the fast path against it.
    bool SphereIntersectsAabb(const DirectX::XMFLOAT3& center, float radius, const ClusterAabb& box);

    class LightClusterer
    {
    public:
        explicit LightClusterer(const ClusterGridDesc& desc = ClusterGridDesc());

        // Prepares the grid for a camera. The cluster boxes only depend on the projection and the screen size, so
        // they are rebuilt only when one of those changes; a moving camera costs nothing here.
        // Returns false when clustering cannot be used (orthographic or degenerate projection, empty screen): the
        // clusterer then assigns nothing and Constants() asks the shader to loop over all lights.
        bool SetCamera(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection,
                       uint32_t screenWidth, uint32_t screenHeight);

        // Assigns the lights (world-space position + range) to clusters. Call after SetCamera, every frame.
        // Allocates nothing once the buffers have reached their size.
        void Assign(const LightGPU* lights, uint32_t count);

        // ---- result
        uint32_t ClusterCount() const { return m_Desc.tilesX * m_Desc.tilesY * m_Desc.slicesZ; }
        const std::vector<ClusterRange>& Ranges() const { return m_Ranges; }       // ClusterCount() entries
        const uint32_t* Indices() const { return m_Indices.data(); }
        uint32_t IndexCount() const { return m_IndexCount; }                        // valid entries of Indices()
        const ClusterStats& Stats() const { return m_Stats; }

        // The constant block for the pixel shader. 'mode' is LightCullingMode; it is forced to 0 (loop over all
        // lights) when the grid is not usable.
        ClusterConstants Constants(uint32_t mode) const;

        // ---- geometry (also used by the tests)
        const ClusterGridDesc& Desc() const { return m_Desc; }
        bool  IsValid() const { return m_Valid; }
        float NearPlane() const { return m_Near; }
        float FarPlane() const { return m_Far; }
        uint32_t ClusterIndex(uint32_t x, uint32_t y, uint32_t z) const { return x + y * m_Desc.tilesX + z * m_Desc.tilesX * m_Desc.tilesY; }
        const ClusterAabb& Bounds(uint32_t clusterIndex) const { return m_Bounds[clusterIndex]; }
        uint32_t SliceOf(float viewZ) const;        // the depth slice of a view-space depth (clamped to the grid)

    private:
        struct LightRange { DirectX::XMFLOAT3 center; float radius; uint32_t x0, x1, y0, y1, z0, z1; bool valid; };

        void BuildBounds();
        bool ComputeRange(const DirectX::XMFLOAT3& center, float radius, LightRange& out) const;

        ClusterGridDesc m_Desc;

        DirectX::XMFLOAT4X4 m_View{};
        DirectX::XMFLOAT4X4 m_Projection{};
        uint32_t m_ScreenW = 0, m_ScreenH = 0;
        bool  m_HaveGrid = false;       // m_Projection / m_Screen* describe the current grid
        bool  m_Valid = false;
        float m_Near = 0, m_Far = 0;
        float m_SliceScale = 0, m_SliceBias = 0;

        std::vector<ClusterAabb>  m_Bounds;      // view-space box of every cluster
        std::vector<LightRange>   m_LightRanges; // scratch, reused every frame
        std::vector<uint32_t>     m_Counts;      // scratch
        std::vector<uint32_t>     m_Cursor;      // scratch

        std::vector<ClusterRange> m_Ranges;
        std::vector<uint32_t>     m_Indices;     // sized to maxLightIndices once
        uint32_t                  m_IndexCount = 0;
        ClusterStats              m_Stats;
    };
}
