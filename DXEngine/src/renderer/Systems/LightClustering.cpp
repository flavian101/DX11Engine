#include "dxpch.h"
#include "LightClustering.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace DXEngine
{
    using namespace DirectX;

    namespace
    {
        // Safety margin when turning a light's extent into a tile / slice range. The exact test per cluster decides
        // what is really assigned; the range only has to be a SUPERSET of it, and float rounding must not be able to
        // cut off a cluster that the exact test would have accepted.
        constexpr float kNdcPad = 1e-3f;
        constexpr float kDepthPad = 1e-3f;

        uint32_t ClampIndex(float v, uint32_t count)
        {
            if (!(v > 0.0f)) return 0;                                  // also catches NaN
            if (v >= static_cast<float>(count)) return count - 1;
            return static_cast<uint32_t>(v);
        }
    }

    // ------------------------------------------------------------------ projection helpers
    bool ProjectionIsPerspective(const XMFLOAT4X4& p)
    {
        // XMMatrixPerspective*LH: _34 = 1 (w takes the view depth), _44 = 0. Orthographic has _34 = 0, _44 = 1.
        return p._34 == 1.0f && p._44 == 0.0f && p._11 > 0.0f && p._22 > 0.0f && p._33 != 0.0f;
    }

    float ProjectionNearPlane(const XMFLOAT4X4& p) { return -p._43 / p._33; }
    float ProjectionFarPlane(const XMFLOAT4X4& p)  { return p._43 / (1.0f - p._33); }

    bool SphereIntersectsAabb(const XMFLOAT3& c, float r, const ClusterAabb& b)
    {
        float d2 = 0.0f;
        const float cv[3] = { c.x, c.y, c.z };
        const float lo[3] = { b.min.x, b.min.y, b.min.z };
        const float hi[3] = { b.max.x, b.max.y, b.max.z };
        for (int i = 0; i < 3; ++i)
        {
            if (cv[i] < lo[i])      { const float d = lo[i] - cv[i]; d2 += d * d; }
            else if (cv[i] > hi[i]) { const float d = cv[i] - hi[i]; d2 += d * d; }
        }
        return d2 <= r * r;
    }

    // ------------------------------------------------------------------ LightClusterer
    LightClusterer::LightClusterer(const ClusterGridDesc& desc)
        : m_Desc(desc)
    {
        m_Desc.tilesX = std::max(m_Desc.tilesX, 1u);
        m_Desc.tilesY = std::max(m_Desc.tilesY, 1u);
        m_Desc.slicesZ = std::max(m_Desc.slicesZ, 1u);
        m_Ranges.assign(ClusterCount(), ClusterRange{ 0, 0 });
        m_Counts.assign(ClusterCount(), 0);
        m_Cursor.assign(ClusterCount(), 0);
        m_Indices.assign(m_Desc.maxLightIndices, 0);
        m_Bounds.resize(ClusterCount());
    }

    bool LightClusterer::SetCamera(const XMFLOAT4X4& view, const XMFLOAT4X4& projection, uint32_t w, uint32_t h)
    {
        m_View = view;                                                  // changes every frame: just remembered

        const bool gridUnchanged = m_HaveGrid && w == m_ScreenW && h == m_ScreenH &&
                                   std::memcmp(&projection, &m_Projection, sizeof(XMFLOAT4X4)) == 0;
        if (gridUnchanged)
            return m_Valid;

        m_Projection = projection;
        m_ScreenW = w;
        m_ScreenH = h;
        m_HaveGrid = true;
        m_Valid = false;

        if (w == 0 || h == 0 || !ProjectionIsPerspective(projection))
            return false;

        m_Near = ProjectionNearPlane(projection);
        m_Far  = ProjectionFarPlane(projection);
        if (!(m_Near > 0.0f) || !(m_Far > m_Near) || !std::isfinite(m_Far))
            return false;

        m_SliceScale = static_cast<float>(m_Desc.slicesZ) / std::log(m_Far / m_Near);
        m_SliceBias  = -std::log(m_Near) * m_SliceScale;

        BuildBounds();
        m_Valid = true;
        return true;
    }

    uint32_t LightClusterer::SliceOf(float viewZ) const
    {
        if (!(viewZ > 0.0f)) return 0;
        return ClampIndex(std::floor(std::log(viewZ) * m_SliceScale + m_SliceBias), m_Desc.slicesZ);
    }

    // The view-space box around every cluster. A cluster is a slice of the frustum between two depths and two tile
    // edges per axis; x (and y) of its corners scale with depth, so the box spans the extremes of the 8 corners.
    void LightClusterer::BuildBounds()
    {
        const float depthRatio = m_Far / m_Near;
        const float p11 = m_Projection._11, p22 = m_Projection._22;
        const float p31 = m_Projection._31, p32 = m_Projection._32;   // non-zero only for an off-centre projection

        for (uint32_t z = 0; z < m_Desc.slicesZ; ++z)
        {
            const float z0 = m_Near * std::pow(depthRatio, static_cast<float>(z) / m_Desc.slicesZ);
            const float z1 = m_Near * std::pow(depthRatio, static_cast<float>(z + 1) / m_Desc.slicesZ);

            for (uint32_t y = 0; y < m_Desc.tilesY; ++y)
            {
                // tile row 0 is the TOP of the screen (ndc y = +1)
                const float ndcYTop = 1.0f - 2.0f * static_cast<float>(y) / m_Desc.tilesY;
                const float ndcYBot = 1.0f - 2.0f * static_cast<float>(y + 1) / m_Desc.tilesY;

                for (uint32_t x = 0; x < m_Desc.tilesX; ++x)
                {
                    const float ndcXLeft  = -1.0f + 2.0f * static_cast<float>(x) / m_Desc.tilesX;
                    const float ndcXRight = -1.0f + 2.0f * static_cast<float>(x + 1) / m_Desc.tilesX;

                    // view x = (ndc.x - p31) * z / p11, over the 4 (edge, depth) combinations
                    const float xs[4] = { (ndcXLeft - p31) * z0 / p11, (ndcXLeft - p31) * z1 / p11,
                                          (ndcXRight - p31) * z0 / p11, (ndcXRight - p31) * z1 / p11 };
                    const float ys[4] = { (ndcYBot - p32) * z0 / p22, (ndcYBot - p32) * z1 / p22,
                                          (ndcYTop - p32) * z0 / p22, (ndcYTop - p32) * z1 / p22 };

                    ClusterAabb& box = m_Bounds[ClusterIndex(x, y, z)];
                    box.min = { *std::min_element(xs, xs + 4), *std::min_element(ys, ys + 4), z0 };
                    box.max = { *std::max_element(xs, xs + 4), *std::max_element(ys, ys + 4), z1 };
                }
            }
        }
    }

    // A fast, conservative bound: the tiles and slices that the sphere's view-space box can touch. (x / z is
    // monotonic in both x and z, so the screen-space extremes of a box are at its corners.)
    bool LightClusterer::ComputeRange(const XMFLOAT3& c, float r, LightRange& out) const
    {
        if (!(r > 0.0f) || !std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z) || !std::isfinite(r))
            return false;
        if (c.z + r < m_Near || c.z - r > m_Far)
            return false;                                               // entirely behind the near plane or beyond far

        // Only the part of the sphere between near and far can be seen.
        const float zA = std::max(c.z - r, m_Near);
        const float zB = std::min(c.z + r, m_Far);

        out.z0 = SliceOf(zA * (1.0f - kDepthPad));
        out.z1 = SliceOf(zB * (1.0f + kDepthPad));

        auto ndcRange = [&](float center, float scale, float offset, float& lo, float& hi)
        {
            const float a = center - r, b = center + r;
            const float v[4] = { a * scale / zA, a * scale / zB, b * scale / zA, b * scale / zB };
            lo = *std::min_element(v, v + 4) + offset;
            hi = *std::max_element(v, v + 4) + offset;
        };
        float xLo, xHi, yLo, yHi;
        ndcRange(c.x, m_Projection._11, m_Projection._31, xLo, xHi);
        ndcRange(c.y, m_Projection._22, m_Projection._32, yLo, yHi);

        if (xHi < -1.0f - kNdcPad || xLo > 1.0f + kNdcPad || yHi < -1.0f - kNdcPad || yLo > 1.0f + kNdcPad)
            return false;                                               // off screen

        out.x0 = ClampIndex(std::floor((xLo * 0.5f + 0.5f - kNdcPad) * m_Desc.tilesX), m_Desc.tilesX);
        out.x1 = ClampIndex(std::floor((xHi * 0.5f + 0.5f + kNdcPad) * m_Desc.tilesX), m_Desc.tilesX);
        out.y0 = ClampIndex(std::floor((0.5f - yHi * 0.5f - kNdcPad) * m_Desc.tilesY), m_Desc.tilesY);   // screen y is flipped
        out.y1 = ClampIndex(std::floor((0.5f - yLo * 0.5f + kNdcPad) * m_Desc.tilesY), m_Desc.tilesY);
        out.center = c;
        out.radius = r;
        return true;
    }

    void LightClusterer::Assign(const LightGPU* lights, uint32_t count)
    {
        m_Stats = ClusterStats{};
        m_IndexCount = 0;
        m_Ranges.assign(ClusterCount(), ClusterRange{ 0, 0 });          // same size every frame: no allocation
        if (!m_Valid || !lights || count == 0)
            return;

        std::fill(m_Counts.begin(), m_Counts.end(), 0u);
        m_LightRanges.resize(count);
        const XMMATRIX view = XMLoadFloat4x4(&m_View);

        auto forEachCluster = [&](const LightRange& lr, auto&& fn)
        {
            for (uint32_t z = lr.z0; z <= lr.z1; ++z)
                for (uint32_t y = lr.y0; y <= lr.y1; ++y)
                    for (uint32_t x = lr.x0; x <= lr.x1; ++x)
                    {
                        const uint32_t cluster = ClusterIndex(x, y, z);
                        if (SphereIntersectsAabb(lr.center, lr.radius, m_Bounds[cluster]))
                            fn(cluster);
                    }
        };

        // ---- pass 1: count how many lights each cluster gets
        for (uint32_t i = 0; i < count; ++i)
        {
            LightRange& lr = m_LightRanges[i];
            lr.valid = false;

            XMFLOAT3 p;
            XMStoreFloat3(&p, XMVector3TransformCoord(XMLoadFloat3(&lights[i].position), view));
            if (ComputeRange(p, lights[i].range, lr))
            {
                forEachCluster(lr, [&](uint32_t cluster) { ++m_Counts[cluster]; lr.valid = true; });
            }
            ++(lr.valid ? m_Stats.lightsInFrustum : m_Stats.lightsCulled);
        }

        // ---- prefix sum into ranges. The list has a fixed capacity: clusters late in the list (far slices) lose
        // lights first, and the loss is counted.
        uint32_t offset = 0;
        for (uint32_t c = 0; c < ClusterCount(); ++c)
        {
            const uint32_t wanted = m_Counts[c];
            const uint32_t room   = m_Desc.maxLightIndices - offset;
            const uint32_t taken  = std::min(wanted, room);

            m_Ranges[c] = { offset, taken };
            offset += taken;

            m_Stats.assignments        += wanted;
            m_Stats.assignmentsDropped += wanted - taken;
            m_Stats.maxLightsPerCluster = std::max(m_Stats.maxLightsPerCluster, taken);
            if (taken > 0) ++m_Stats.nonEmptyClusters;
        }
        m_IndexCount = offset;

        // ---- pass 2: write the light indices (ascending light order inside every cluster: deterministic)
        std::fill(m_Cursor.begin(), m_Cursor.end(), 0u);
        for (uint32_t i = 0; i < count; ++i)
        {
            const LightRange& lr = m_LightRanges[i];
            if (!lr.valid) continue;
            forEachCluster(lr, [&](uint32_t cluster)
            {
                const ClusterRange& range = m_Ranges[cluster];
                if (m_Cursor[cluster] < range.count)
                    m_Indices[range.offset + m_Cursor[cluster]++] = i;
            });
        }
    }

    ClusterConstants LightClusterer::Constants(uint32_t mode) const
    {
        ClusterConstants c{};
        c.gridX = m_Desc.tilesX;
        c.gridY = m_Desc.tilesY;
        c.gridZ = m_Desc.slicesZ;
        c.mode  = m_Valid ? mode : 0u;
        if (m_ScreenW > 0 && m_ScreenH > 0)
        {
            c.tilesPerPixelX = static_cast<float>(m_Desc.tilesX) / static_cast<float>(m_ScreenW);
            c.tilesPerPixelY = static_cast<float>(m_Desc.tilesY) / static_cast<float>(m_ScreenH);
        }
        c.sliceScale = m_SliceScale;
        c.sliceBias  = m_SliceBias;
        return c;
    }
}
