#pragma once
#include "utils/ConstantBufferTypes.h"
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // ShadowMath: the geometry of shadow maps, with no scene, no GPU and no engine classes: plain numbers in,
    // plain numbers out, so every function here is unit-tested (tests/shadow_tests.cpp).
    //
    //   directional light  -> FitCascade: a tight, STABLE orthographic box around a slice of the camera frustum
    //   spot light         -> BuildSpotView: one perspective view down the cone
    //   point light        -> BuildPointFace: six 90-degree views (+X -X +Y -Y +Z -Z), one per atlas tile
    //   atlas              -> AtlasAllocator: hands out power-of-two square tiles of one big depth texture
    //
    // All matrices are DirectXMath's row-vector convention (clip = v * M), left-handed, and NOT transposed.
    // ==================================================================
    namespace ShadowMath
    {
        uint32_t FloorPow2(uint32_t v);                         // largest power of two <= v (0 -> 0)

        // "Practical split scheme": a blend of uniform and logarithmic splits. outFar[i] = the view-space depth at the
        // FAR edge of cascade i; the last entry is exactly farZ. lambda 0 = uniform, 1 = logarithmic.
        void ComputeCascadeSplits(float nearZ, float farZ, uint32_t count, float lambda, float* outFar);

        // ---------------- directional light ----------------
        struct CascadeFit
        {
            DirectX::XMFLOAT4X4        viewProj;        // world -> light clip
            DirectX::XMFLOAT3          center;          // the bounding sphere of the camera slice, world space
            float                      radius = 0.0f;   // that sphere's radius (rounded UP to 1/16: constant under camera moves)
            float                      texelWorldSize = 0.0f;   // world size of one shadow texel
            float                      depthRange = 0.0f;       // far - near of the light's orthographic depth range
            DirectX::BoundingOrientedBox cullBox;       // everything inside can land in this cascade's map
        };

        // Fits an orthographic shadow box around the camera frustum between view depths sliceNear..sliceFar.
        //
        // Two things keep the shadow from SHIMMERING while the camera moves or turns:
        //  1. the box is a SPHERE-fit: its size depends only on the slice's depths and the field of view, never on how the
        //     camera is oriented, so a shadow texel always covers the same world area;
        //  2. (snapToTexels) the box is moved in whole-texel steps, so a point always falls at the same place inside a texel.
        // The box reaches casterExtent toward the light so casters outside the camera's view still cast into it.
        CascadeFit FitCascade(const DirectX::XMFLOAT4X4& cameraView, float tanHalfX, float tanHalfY,
                              float sliceNear, float sliceFar, const DirectX::XMFLOAT3& lightDirection,
                              uint32_t tileSize, float casterExtent, bool snapToTexels);

        // ---------------- spot + point lights ----------------
        struct PerspectiveView
        {
            DirectX::XMFLOAT4X4    viewProj;
            DirectX::BoundingFrustum frustum;           // world space: casters outside cannot reach this view
            float                  texelWorldAtUnit = 0.0f;   // world size of one texel at distance 1 from the light
            float                  nearZ = 0.0f;
            float                  farZ  = 0.0f;
        };

        // The tangent of the half field of view of a point-light face. It is a little over 1 (a plain 90 degree face):
        // the extra border keeps the PCF kernel of a pixel near a face edge INSIDE its own tile instead of reading the
        // neighbour's. A point on the dominant-axis boundary lands (pcfRadius + 1) texels inside the tile.
        float PointFaceTanHalf(uint32_t tileSize, uint32_t pcfRadius);

        PerspectiveView BuildSpotView(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction,
                                      float outerConeRadians, float range, uint32_t tileSize, uint32_t pcfRadius);

        // face: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z
        PerspectiveView BuildPointFace(const DirectX::XMFLOAT3& position, uint32_t face, float range,
                                       uint32_t tileSize, uint32_t pcfRadius);

        // Which face a direction (fragment - light position) belongs to: the dominant axis. The HLSL does the same.
        uint32_t PointFaceIndex(const DirectX::XMFLOAT3& d);

        // ---------------- the shader's matrix multiply, on the CPU ----------------
        // clip = r0*v.x + r1*v.y + r2*v.z + r3*v.w   (what ShadowView does in common.hlsli). Tested against XMVector4Transform.
        DirectX::XMFLOAT4 MulRows(const DirectX::XMFLOAT4& v, const DirectX::XMFLOAT4X4& m);

        // ---------------- the atlas ----------------
        struct AtlasRect { uint32_t x = 0, y = 0, size = 0; };

        // A quadtree over a square power-of-two atlas. Allocate() returns a square power-of-two tile; free blocks are
        // split in four as needed. Rebuilt every frame (Reset), so there is never any fragmentation to manage.
        // Ask for the BIG tiles first: a small tile splits a large free block, and a later big request may then not fit.
        class AtlasAllocator
        {
        public:
            void     Reset(uint32_t atlasSize, uint32_t minTile);      // atlasSize is rounded down to a power of two
            bool     Allocate(uint32_t size, AtlasRect& out);          // size rounded UP to a power of two, >= minTile
            uint32_t Size() const { return m_Size; }
            uint64_t UsedTexels() const { return m_Used; }

        private:
            uint32_t m_Size = 0;
            uint32_t m_Min  = 1;
            uint64_t m_Used = 0;
            std::vector<std::vector<AtlasRect>> m_Free;               // index = log2(tile size)
        };

        // The GPU element of one view. perspective: params.y is the texel size at distance 1 (the shader scales it).
        ShadowViewGPU MakeGpuView(const DirectX::XMFLOAT4X4& viewProj, const AtlasRect& tile, uint32_t atlasSize,
                                  float normalBiasTexels, float texelWorldSize, float depthBias, bool perspective);
    }
}
