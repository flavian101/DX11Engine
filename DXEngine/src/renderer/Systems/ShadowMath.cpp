#include "dxpch.h"
#include "ShadowMath.h"
#include <algorithm>
#include <cmath>

namespace DXEngine
{
    using namespace DirectX;

    namespace ShadowMath
    {
        namespace
        {
            constexpr float kPi = 3.14159265358979f;

            uint32_t Log2(uint32_t v) { uint32_t l = 0; while ((1u << (l + 1)) <= v && l < 31) ++l; return l; }

            XMVECTOR SafeUp(FXMVECTOR dir)
            {
                // any up vector that is not parallel to the look direction
                return std::fabs(XMVectorGetY(dir)) > 0.99f ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);
            }

            PerspectiveView BuildPerspective(FXMVECTOR position, FXMVECTOR direction, FXMVECTOR up, float tanHalf, float range, uint32_t tileSize)
            {
                PerspectiveView out;
                out.nearZ = std::max(0.05f, range * 0.01f);
                out.farZ  = std::max(range, out.nearZ + 0.01f);

                const float fov = 2.0f * std::atan(tanHalf);
                const XMMATRIX view = XMMatrixLookToLH(position, direction, up);
                const XMMATRIX proj = XMMatrixPerspectiveFovLH(fov, 1.0f, out.nearZ, out.farZ);
                XMStoreFloat4x4(&out.viewProj, XMMatrixMultiply(view, proj));

                BoundingFrustum::CreateFromMatrix(out.frustum, proj);
                out.frustum.Transform(out.frustum, XMMatrixInverse(nullptr, view));   // view space -> world space
                out.texelWorldAtUnit = 2.0f * tanHalf / static_cast<float>(std::max(1u, tileSize));
                return out;
            }
        }

        uint32_t FloorPow2(uint32_t v)
        {
            if (v == 0) return 0;
            return 1u << Log2(v);
        }

        void ComputeCascadeSplits(float nearZ, float farZ, uint32_t count, float lambda, float* outFar)
        {
            count = std::clamp(count, 1u, 4u);
            nearZ = std::max(nearZ, 0.001f);
            farZ  = std::max(farZ, nearZ + 0.001f);
            lambda = std::clamp(lambda, 0.0f, 1.0f);

            for (uint32_t i = 1; i <= count; ++i)
            {
                const float p = static_cast<float>(i) / static_cast<float>(count);
                const float logSplit = nearZ * std::pow(farZ / nearZ, p);
                const float uniSplit = nearZ + (farZ - nearZ) * p;
                outFar[i - 1] = lambda * logSplit + (1.0f - lambda) * uniSplit;
            }
            outFar[count - 1] = farZ;                 // exactly, whatever the rounding did
        }

        CascadeFit FitCascade(const XMFLOAT4X4& cameraView, float tanHalfX, float tanHalfY,
                              float sliceNear, float sliceFar, const XMFLOAT3& lightDirection,
                              uint32_t tileSize, float casterExtent, bool snapToTexels)
        {
            CascadeFit fit;
            tileSize = std::max(tileSize, 8u);

            // ---- the camera's basis in world space (rows of the inverse view matrix)
            const XMMATRIX invView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&cameraView));
            const XMVECTOR camRight = invView.r[0], camUp = invView.r[1], camFwd = invView.r[2], camPos = invView.r[3];
            (void)camRight; (void)camUp;

            // ---- smallest sphere around the slice that sits on the view axis: equidistant to the near and far corners.
            // (center depth c = (n + f)(1 + k^2) / 2 with k^2 = tx^2 + ty^2; past the far plane it is clamped to f)
            const float n = sliceNear, f = std::max(sliceFar, sliceNear + 0.001f);
            const float k2 = tanHalfX * tanHalfX + tanHalfY * tanHalfY;
            float c = 0.5f * (n + f) * (1.0f + k2);
            c = std::min(c, f);
            float radius = std::max(std::sqrt((f - c) * (f - c) + f * f * k2), std::sqrt((c - n) * (c - n) + n * n * k2));
            radius = std::ceil(radius * 16.0f) / 16.0f;            // constant under camera moves, immune to float jitter

            // two texels of room: snapping may move the box by up to a texel
            const float half = radius * static_cast<float>(tileSize) / static_cast<float>(tileSize - 4);
            const float texel = 2.0f * half / static_cast<float>(tileSize);

            const XMVECTOR centerWS = XMVectorAdd(camPos, XMVectorScale(camFwd, c));
            XMStoreFloat3(&fit.center, centerWS);
            fit.radius = radius;
            fit.texelWorldSize = texel;

            // ---- light space: eye at the world origin looking along the light, so snapping works on plain numbers
            const XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&lightDirection));
            const XMMATRIX lightView = XMMatrixLookToLH(XMVectorZero(), dir, SafeUp(dir));

            XMFLOAT3 cLS;
            XMStoreFloat3(&cLS, XMVector3TransformCoord(centerWS, lightView));
            if (snapToTexels)
            {
                cLS.x = std::floor(cLS.x / texel) * texel;
                cLS.y = std::floor(cLS.y / texel) * texel;
            }

            const float zNear = cLS.z - half - casterExtent;       // toward the light: casters outside the view
            const float zFar  = cLS.z + half;
            fit.depthRange = zFar - zNear;

            const XMMATRIX proj = XMMatrixOrthographicOffCenterLH(cLS.x - half, cLS.x + half, cLS.y - half, cLS.y + half, zNear, zFar);
            XMStoreFloat4x4(&fit.viewProj, XMMatrixMultiply(lightView, proj));

            // ---- the same box as an oriented box in world space (for culling casters on the CPU)
            const XMMATRIX lightToWorld = XMMatrixTranspose(lightView);          // rotation only: transpose = inverse
            const XMVECTOR boxCenterLS = XMVectorSet(cLS.x, cLS.y, 0.5f * (zNear + zFar), 1.0f);
            XMFLOAT3 boxCenterWS, extents{ half, half, 0.5f * (zFar - zNear) };
            XMStoreFloat3(&boxCenterWS, XMVector3TransformCoord(boxCenterLS, lightToWorld));
            XMFLOAT4 q;
            XMStoreFloat4(&q, XMQuaternionRotationMatrix(lightToWorld));          // rows = the light's right / up / forward
            fit.cullBox = BoundingOrientedBox(boxCenterWS, extents, q);
            return fit;
        }

        float PointFaceTanHalf(uint32_t tileSize, uint32_t pcfRadius)
        {
            tileSize = std::max(tileSize, 8u);
            const float margin = static_cast<float>(pcfRadius + 1);
            return 1.0f / (1.0f - 2.0f * margin / static_cast<float>(tileSize));
        }

        PerspectiveView BuildSpotView(const XMFLOAT3& position, const XMFLOAT3& direction, float outerConeRadians,
                                      float range, uint32_t tileSize, uint32_t pcfRadius)
        {
            tileSize = std::max(tileSize, 8u);
            const float margin = static_cast<float>(pcfRadius + 1);
            float tanHalf = std::tan(std::clamp(outerConeRadians, 0.01f, 85.0f * kPi / 180.0f)) / (1.0f - 2.0f * margin / static_cast<float>(tileSize));
            tanHalf = std::min(tanHalf, std::tan(85.0f * kPi / 180.0f));              // fov <= 170 degrees

            const XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&direction));
            return BuildPerspective(XMLoadFloat3(&position), dir, SafeUp(dir), tanHalf, range, tileSize);
        }

        PerspectiveView BuildPointFace(const XMFLOAT3& position, uint32_t face, float range, uint32_t tileSize, uint32_t pcfRadius)
        {
            static const float kDir[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
            static const float kUp[6][3]  = { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 0 } };
            face = std::min(face, 5u);
            return BuildPerspective(XMLoadFloat3(&position), XMVectorSet(kDir[face][0], kDir[face][1], kDir[face][2], 0),
                                    XMVectorSet(kUp[face][0], kUp[face][1], kUp[face][2], 0),
                                    PointFaceTanHalf(tileSize, pcfRadius), range, tileSize);
        }

        uint32_t PointFaceIndex(const XMFLOAT3& d)
        {
            const float ax = std::fabs(d.x), ay = std::fabs(d.y), az = std::fabs(d.z);
            if (ax >= ay && ax >= az) return d.x > 0.0f ? 0u : 1u;
            if (ay >= az)             return d.y > 0.0f ? 2u : 3u;
            return d.z > 0.0f ? 4u : 5u;
        }

        XMFLOAT4 MulRows(const XMFLOAT4& v, const XMFLOAT4X4& m)
        {
            return XMFLOAT4{ m._11 * v.x + m._21 * v.y + m._31 * v.z + m._41 * v.w,
                             m._12 * v.x + m._22 * v.y + m._32 * v.z + m._42 * v.w,
                             m._13 * v.x + m._23 * v.y + m._33 * v.z + m._43 * v.w,
                             m._14 * v.x + m._24 * v.y + m._34 * v.z + m._44 * v.w };
        }

        // ---------------- atlas ----------------
        void AtlasAllocator::Reset(uint32_t atlasSize, uint32_t minTile)
        {
            m_Size = FloorPow2(atlasSize);
            m_Min  = std::max(1u, FloorPow2(minTile));
            m_Used = 0;
            m_Free.assign(m_Size ? Log2(m_Size) + 1 : 0, {});
            if (m_Size) m_Free[Log2(m_Size)].push_back(AtlasRect{ 0, 0, m_Size });
        }

        bool AtlasAllocator::Allocate(uint32_t size, AtlasRect& out)
        {
            if (m_Size == 0 || size > m_Size) return false;
            size = std::max(m_Min, size);
            uint32_t pow2 = FloorPow2(size);
            if (pow2 < size) pow2 <<= 1;                 // round UP
            if (pow2 > m_Size) return false;

            const uint32_t want = Log2(pow2);
            uint32_t level = want;
            while (level < m_Free.size() && m_Free[level].empty()) ++level;
            if (level >= m_Free.size()) return false;    // nothing big enough is free

            AtlasRect block = m_Free[level].back();
            m_Free[level].pop_back();

            while (level > want)                         // split in four, keep the first, free the other three
            {
                --level;
                const uint32_t h = block.size / 2;
                m_Free[level].push_back(AtlasRect{ block.x + h, block.y + h, h });
                m_Free[level].push_back(AtlasRect{ block.x,     block.y + h, h });
                m_Free[level].push_back(AtlasRect{ block.x + h, block.y,     h });
                block = AtlasRect{ block.x, block.y, h };
            }

            out = block;
            m_Used += static_cast<uint64_t>(pow2) * pow2;
            return true;
        }

        ShadowViewGPU MakeGpuView(const XMFLOAT4X4& viewProj, const AtlasRect& tile, uint32_t atlasSize,
                                  float normalBiasTexels, float texelWorldSize, float depthBias, bool perspective)
        {
            const float inv = 1.0f / static_cast<float>(std::max(1u, atlasSize));
            ShadowViewGPU g;
            g.viewProj  = viewProj;
            g.atlasRect = XMFLOAT4{ tile.x * inv, tile.y * inv, tile.size * inv, tile.size * inv };
            g.params    = XMFLOAT4{ normalBiasTexels, texelWorldSize, depthBias, perspective ? 1.0f : 0.0f };
            return g;
        }
    }
}
