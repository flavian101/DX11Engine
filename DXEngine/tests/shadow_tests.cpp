// Shadows without a GPU: the geometry of shadow maps (cascade fitting, spot / point views, the atlas allocator) and the
// planner that decides which lights get shadows. The important tests check PROPERTIES the pixel shader relies on:
// every pixel of the camera slice lands inside its cascade, a cascade does not shimmer when the camera moves, a point
// light's six faces cover every direction with room for the PCF kernel, the atlas never overlaps.
#include "renderer/Systems/ShadowMath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace DXEngine;
using namespace DirectX;
using namespace DXEngine::ShadowMath;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAILED line %d: %s\n", __LINE__, #cond); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

// ------------------------------------------------------------ helpers
static XMFLOAT4X4 Store(FXMMATRIX m) { XMFLOAT4X4 f; XMStoreFloat4x4(&f, m); return f; }

// camera at `pos`, looking by yaw/pitch (radians)
static XMFLOAT4X4 CameraView(XMFLOAT3 pos, float yaw = 0.0f, float pitch = 0.0f)
{
    const XMVECTOR dir = XMVectorSet(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch), 0);
    return Store(XMMatrixLookToLH(XMLoadFloat3(&pos), dir, XMVectorSet(0, 1, 0, 0)));
}

static constexpr float kFovDeg = 60.0f, kAspect = 16.0f / 9.0f;
static float TanY() { return std::tan(XMConvertToRadians(kFovDeg) * 0.5f); }
static float TanX() { return TanY() * kAspect; }

// the 8 corners of the camera frustum slice [n, f] in world space
static std::vector<XMFLOAT3> SliceCorners(const XMFLOAT4X4& view, float n, float f)
{
    const XMMATRIX inv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view));
    std::vector<XMFLOAT3> out;
    for (float d : { n, f })
        for (float sx : { -1.0f, 1.0f })
            for (float sy : { -1.0f, 1.0f })
            {
                XMFLOAT3 p;
                XMStoreFloat3(&p, XMVector3TransformCoord(XMVectorSet(sx * TanX() * d, sy * TanY() * d, d, 1), inv));
                out.push_back(p);
            }
    return out;
}

// world point -> NDC through a shadow view matrix, with the SAME multiply the shader uses
static XMFLOAT3 ToNdc(const XMFLOAT4X4& vp, XMFLOAT3 p)
{
    const XMFLOAT4 c = MulRows(XMFLOAT4{ p.x, p.y, p.z, 1.0f }, vp);
    return XMFLOAT3{ c.x / c.w, c.y / c.w, c.z / c.w };
}

static float Frac(float v) { return v - std::floor(v); }

// ------------------------------------------------------------ pure helpers
static void TestFloorPow2()
{
    CHECK(FloorPow2(0) == 0);
    CHECK(FloorPow2(1) == 1);
    CHECK(FloorPow2(1000) == 512);
    CHECK(FloorPow2(1024) == 1024);
    CHECK(FloorPow2(4095) == 2048);
    CHECK(FloorPow2(4096) == 4096);
}

static void TestMulRowsMatchesDirectXMath()
{
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> d(-5.0f, 5.0f);
    for (int i = 0; i < 50; ++i)
    {
        const XMFLOAT4X4 m = Store(XMMatrixRotationRollPitchYaw(d(rng), d(rng), d(rng)) * XMMatrixTranslation(d(rng), d(rng), d(rng)) *
                                   XMMatrixPerspectiveFovLH(1.0f, 1.3f, 0.1f, 50.0f));
        const XMFLOAT4 v{ d(rng), d(rng), d(rng), 1.0f };
        XMFLOAT4 ref;
        XMStoreFloat4(&ref, XMVector4Transform(XMLoadFloat4(&v), XMLoadFloat4x4(&m)));
        const XMFLOAT4 got = MulRows(v, m);
        CHECK(Near(got.x, ref.x, 1e-3f) && Near(got.y, ref.y, 1e-3f) && Near(got.z, ref.z, 1e-3f) && Near(got.w, ref.w, 1e-3f));
    }
}

static void TestCascadeSplits()
{
    float s[4];
    ComputeCascadeSplits(0.5f, 100.0f, 4, 0.75f, s);
    CHECK(s[0] > 0.5f && s[0] < s[1] && s[1] < s[2] && s[2] < s[3]);
    CHECK(s[3] == 100.0f);                                   // the last split is EXACTLY the far distance

    ComputeCascadeSplits(1.0f, 101.0f, 4, 0.0f, s);          // lambda 0: uniform
    CHECK(Near(s[0], 26.0f) && Near(s[1], 51.0f) && Near(s[2], 76.0f) && Near(s[3], 101.0f));

    ComputeCascadeSplits(1.0f, 16.0f, 4, 1.0f, s);           // lambda 1: logarithmic, ratio 2 each
    CHECK(Near(s[0], 2.0f) && Near(s[1], 4.0f) && Near(s[2], 8.0f) && Near(s[3], 16.0f));

    ComputeCascadeSplits(0.5f, 50.0f, 1, 0.75f, s);          // one cascade covers everything
    CHECK(s[0] == 50.0f);

    ComputeCascadeSplits(0.5f, 50.0f, 99, 0.75f, s);         // clamped to the maximum (4): must not write out of bounds
    CHECK(s[3] == 50.0f);
}

// ------------------------------------------------------------ directional light
static void TestCascadeContainsItsSlice()
{
    const XMFLOAT3 lights[] = { { 0.3f, -1.0f, 0.2f }, { 1.0f, -0.2f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { -0.5f, -0.5f, 0.7f } };
    for (bool snap : { false, true })
        for (const XMFLOAT3& L : lights)
            for (float yaw : { 0.0f, 1.3f, 3.0f })
            {
                const XMFLOAT4X4 view = CameraView({ 12, 3, -8 }, yaw, -0.2f);
                const float n = 5.0f, f = 40.0f;
                const CascadeFit fit = FitCascade(view, TanX(), TanY(), n, f, L, 1024, 150.0f, snap);
                for (const XMFLOAT3& c : SliceCorners(view, n, f))
                {
                    const XMFLOAT3 ndc = ToNdc(fit.viewProj, c);
                    CHECK(std::fabs(ndc.x) <= 1.0f && std::fabs(ndc.y) <= 1.0f && ndc.z >= 0.0f && ndc.z <= 1.0f);
                    // ... and with the 2-texel safety border the shadow keeps for snapping
                    CHECK(std::fabs(ndc.x) <= 1.0f - 2.0f / 1024.0f + 1e-4f && std::fabs(ndc.y) <= 1.0f - 2.0f / 1024.0f + 1e-4f);
                }
            }
}

static void TestCascadeSizeIgnoresCameraOrientation()
{
    // The shimmer fix #1: the box depends on the slice's depths and the field of view, NOT on where the camera looks.
    const XMFLOAT3 L{ 0.3f, -1.0f, 0.2f };
    const CascadeFit a = FitCascade(CameraView({ 0, 2, 0 }, 0.0f, 0.0f), TanX(), TanY(), 4, 60, L, 1024, 150, true);
    for (float yaw : { 0.7f, 2.0f, 4.1f })
        for (float pitch : { -0.6f, 0.0f, 0.9f })
        {
            const CascadeFit b = FitCascade(CameraView({ 31, -4, 17 }, yaw, pitch), TanX(), TanY(), 4, 60, L, 1024, 150, true);
            CHECK(a.radius == b.radius);
            CHECK(a.texelWorldSize == b.texelWorldSize);
            CHECK(a.depthRange == b.depthRange);
        }
}

static void TestCascadeSnapsToTexels()
{
    // The shimmer fix #2: move the camera in tiny steps. A fixed world point must keep the SAME position inside its
    // shadow texel (fractional texel coordinate), or its shadow edge flickers as the camera walks.
    const XMFLOAT3 L{ 0.4f, -1.0f, 0.3f };
    const XMFLOAT3 probe{ 7.3f, 0.2f, 21.9f };
    const uint32_t tile = 1024;

    float fracX[2] = { 0, 0 }, fracY[2] = { 0, 0 };
    float spreadSnapped = 0.0f, spreadFree = 0.0f;
    for (int mode = 0; mode < 2; ++mode)                     // 0 = snapped, 1 = free
    {
        bool first = true;
        float minX = 0, maxX = 0, minY = 0, maxY = 0;
        for (int i = 0; i < 40; ++i)
        {
            const XMFLOAT4X4 view = CameraView({ 0.0173f * i, 2.0f, 0.0311f * i }, 0.5f, 0.0f);
            const CascadeFit fit = FitCascade(view, TanX(), TanY(), 4, 60, L, tile, 150, mode == 0);
            const XMFLOAT3 ndc = ToNdc(fit.viewProj, probe);
            const float tx = Frac(ndc.x * 0.5f * tile), ty = Frac(ndc.y * 0.5f * tile);
            if (first) { minX = maxX = tx; minY = maxY = ty; first = false; }
            minX = std::min(minX, tx); maxX = std::max(maxX, tx); minY = std::min(minY, ty); maxY = std::max(maxY, ty);
        }
        fracX[mode] = maxX - minX; fracY[mode] = maxY - minY;
    }
    spreadSnapped = std::max(fracX[0], fracY[0]);
    spreadFree    = std::max(fracX[1], fracY[1]);
    CHECK(spreadSnapped < 0.02f);                            // snapped: the texel phase does not move
    CHECK(spreadFree > 0.2f);                                // unsnapped: it does (the test can fail, so it means something)
}

static void TestCascadeCullBoxMatchesTheView()
{
    // The CPU culls casters with cullBox, the GPU renders with viewProj: they must describe the same volume.
    const XMFLOAT3 L{ 0.3f, -1.0f, 0.2f };
    const XMFLOAT4X4 view = CameraView({ 5, 3, -2 }, 1.1f, -0.3f);
    const CascadeFit fit = FitCascade(view, TanX(), TanY(), 3, 50, L, 1024, 150, true);

    std::mt19937 rng(11);
    std::uniform_real_distribution<float> d(-120.0f, 120.0f);
    int inside = 0, outside = 0;
    for (int i = 0; i < 4000; ++i)
    {
        const XMFLOAT3 p{ d(rng) + fit.center.x, d(rng) + fit.center.y, d(rng) + fit.center.z };
        const XMFLOAT3 ndc = ToNdc(fit.viewProj, p);
        const bool clearlyInside  = std::fabs(ndc.x) < 0.98f && std::fabs(ndc.y) < 0.98f && ndc.z > 0.01f && ndc.z < 0.99f;
        const bool clearlyOutside = std::fabs(ndc.x) > 1.03f || std::fabs(ndc.y) > 1.03f || ndc.z < -0.02f || ndc.z > 1.02f;
        const bool boxHasIt = fit.cullBox.Contains(XMLoadFloat3(&p)) != DISJOINT;
        if (clearlyInside)  { ++inside;  CHECK(boxHasIt); }
        if (clearlyOutside) { ++outside; CHECK(!boxHasIt); }
    }
    CHECK(inside > 50 && outside > 1000);                    // both branches really ran
}

static void TestCascadeReachesCastersTowardTheLight()
{
    // a caster far BEHIND the camera slice, toward the light, still has to be in the cull box (it shadows the slice)
    const XMFLOAT3 L{ 0.0f, -1.0f, 0.0f };                   // straight down
    const XMFLOAT4X4 view = CameraView({ 0, 1, 0 }, 0.0f, 0.0f);
    const CascadeFit fit = FitCascade(view, TanX(), TanY(), 0.5f, 30, L, 1024, 100.0f, true);
    const XMFLOAT3 ground{ 0, 0, 10 };                                       // in the slice
    const XMFLOAT3 tallRoof{ 0, 80, 10 };                                    // 80 m above it: casts a shadow onto it
    const XMFLOAT3 tooHigh{ 0, 400, 10 };                                    // beyond casterExtent
    CHECK(fit.cullBox.Contains(XMLoadFloat3(&ground)) != DISJOINT);
    CHECK(fit.cullBox.Contains(XMLoadFloat3(&tallRoof)) != DISJOINT);
    CHECK(fit.cullBox.Contains(XMLoadFloat3(&tooHigh)) == DISJOINT);
}

// ------------------------------------------------------------ spot + point
static void TestSpotView()
{
    const XMFLOAT3 pos{ 3, 6, -2 }, dir{ 0, -1, 0 };
    const float outer = XMConvertToRadians(35.0f);
    const PerspectiveView v = BuildSpotView(pos, dir, outer, 20.0f, 1024, 1);

    const XMFLOAT3 onAxis = ToNdc(v.viewProj, { 3, 1, -2 });                  // 5 m down the axis
    CHECK(Near(onAxis.x, 0.0f, 1e-3f) && Near(onAxis.y, 0.0f, 1e-3f) && onAxis.z > 0.0f && onAxis.z < 1.0f);

    // a point exactly on the outer cone edge, 10 m away, stays inside with the PCF border
    const float r = 10.0f;
    const XMFLOAT3 edge{ pos.x + std::sin(outer) * r, pos.y - std::cos(outer) * r, pos.z };
    const XMFLOAT3 e = ToNdc(v.viewProj, edge);
    CHECK(std::fabs(e.x) < 1.0f - 2.0f * 2.0f / 1024.0f + 1e-3f && std::fabs(e.y) < 1.0f);

    // the culling frustum agrees: inside the cone yes, behind the light no, beyond the range no
    const XMFLOAT3 inCone{ 3, 1, -2 }, behind{ 3, 9, -2 }, tooFar{ 3, -30, -2 };
    CHECK(v.frustum.Contains(XMLoadFloat3(&inCone)) != DISJOINT);
    CHECK(v.frustum.Contains(XMLoadFloat3(&behind)) == DISJOINT);
    CHECK(v.frustum.Contains(XMLoadFloat3(&tooFar)) == DISJOINT);
    CHECK(Near(v.farZ, 20.0f) && v.nearZ > 0.0f && v.nearZ < 1.0f);

    // a very wide spot never produces a degenerate (>= 180 degree) projection
    const PerspectiveView wide = BuildSpotView(pos, dir, XMConvertToRadians(89.0f), 20.0f, 1024, 1);
    CHECK(std::isfinite(wide.viewProj._11) && wide.viewProj._11 > 0.0f);
}

static void TestPointFacesCoverEveryDirection()
{
    const XMFLOAT3 pos{ -4, 2, 9 };
    for (uint32_t pcf : { 0u, 1u, 3u })
        for (uint32_t tile : { 256u, 512u })
        {
            PerspectiveView faces[6];
            for (uint32_t f = 0; f < 6; ++f) faces[f] = BuildPointFace(pos, f, 30.0f, tile, pcf);

            const float border = 2.0f * static_cast<float>(pcf + 1) / static_cast<float>(tile);   // NDC size of the PCF border
            std::mt19937 rng(5 + pcf);
            std::uniform_real_distribution<float> d(-1.0f, 1.0f);
            for (int i = 0; i < 3000; ++i)
            {
                XMFLOAT3 dir{ d(rng), d(rng), d(rng) };
                const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                if (len < 0.05f) continue;
                const float dist = 1.0f + 25.0f * (0.5f + 0.5f * d(rng));
                dir = { dir.x / len * dist, dir.y / len * dist, dir.z / len * dist };

                const uint32_t face = PointFaceIndex(dir);
                const XMFLOAT3 ndc = ToNdc(faces[face].viewProj, { pos.x + dir.x, pos.y + dir.y, pos.z + dir.z });
                // the face the shader picks contains the point, with the whole PCF kernel inside the tile
                CHECK(std::fabs(ndc.x) <= 1.0f - border + 1e-4f && std::fabs(ndc.y) <= 1.0f - border + 1e-4f);
                CHECK(ndc.z > 0.0f && ndc.z < 1.0f);
            }
        }
    CHECK(PointFaceIndex({ 1, 0, 0 }) == 0 && PointFaceIndex({ -1, 0, 0 }) == 1 && PointFaceIndex({ 0, 1, 0 }) == 2 &&
          PointFaceIndex({ 0, -1, 0 }) == 3 && PointFaceIndex({ 0, 0, 1 }) == 4 && PointFaceIndex({ 0, 0, -1 }) == 5);
}

// ------------------------------------------------------------ atlas
static bool Overlap(const AtlasRect& a, const AtlasRect& b)
{
    return a.x < b.x + b.size && b.x < a.x + a.size && a.y < b.y + b.size && b.y < a.y + a.size;
}

static void TestAtlasPacksExactly()
{
    AtlasAllocator atlas;
    atlas.Reset(4096, 128);
    std::vector<AtlasRect> r;
    for (int i = 0; i < 16; ++i) { AtlasRect t; CHECK(atlas.Allocate(1024, t)); r.push_back(t); }
    AtlasRect extra;
    CHECK(!atlas.Allocate(1024, extra));                     // the 17th does not fit
    CHECK(!atlas.Allocate(128, extra));                      // not even the smallest: it is full
    CHECK(atlas.UsedTexels() == 4096ull * 4096ull);

    for (size_t i = 0; i < r.size(); ++i)
    {
        CHECK(r[i].x + r[i].size <= 4096 && r[i].y + r[i].size <= 4096);
        CHECK(r[i].x % 1024 == 0 && r[i].y % 1024 == 0);
        for (size_t j = i + 1; j < r.size(); ++j) CHECK(!Overlap(r[i], r[j]));
    }
}

static void TestAtlasMixedSizesNeverOverlap()
{
    std::mt19937 rng(3);
    const uint32_t sizes[] = { 128, 256, 512, 1024, 2048 };
    for (int round = 0; round < 40; ++round)
    {
        AtlasAllocator atlas;
        atlas.Reset(4096, 128);
        std::vector<AtlasRect> r;
        uint64_t expectUsed = 0;
        for (int i = 0; i < 60; ++i)
        {
            AtlasRect t;
            const uint32_t want = sizes[rng() % 5];
            if (atlas.Allocate(want, t))
            {
                CHECK(t.size == want);
                CHECK(t.x % t.size == 0 && t.y % t.size == 0);                       // tiles are aligned to their size
                CHECK(t.x + t.size <= 4096 && t.y + t.size <= 4096);
                for (const AtlasRect& o : r) CHECK(!Overlap(t, o));
                r.push_back(t);
                expectUsed += static_cast<uint64_t>(want) * want;
            }
        }
        CHECK(atlas.UsedTexels() == expectUsed);
        CHECK(!r.empty());
    }
}

static void TestAtlasRoundingAndReset()
{
    AtlasAllocator atlas;
    atlas.Reset(5000, 128);                                  // not a power of two: rounded down
    CHECK(atlas.Size() == 4096);

    AtlasRect t;
    CHECK(atlas.Allocate(600, t) && t.size == 1024);         // rounded UP: asking for 600 gives 1024
    CHECK(atlas.Allocate(10, t) && t.size == 128);           // below the minimum: the minimum
    CHECK(!atlas.Allocate(8192, t));                         // bigger than the atlas

    atlas.Reset(2048, 128);                                  // a new frame starts from nothing
    CHECK(atlas.UsedTexels() == 0 && atlas.Allocate(2048, t) && t.x == 0 && t.y == 0);

    AtlasAllocator empty;                                    // never initialised: refuses, does not crash
    CHECK(!empty.Allocate(128, t));
}

static void TestGpuViewEncoding()
{
    const XMFLOAT4X4 id = Store(XMMatrixIdentity());
    const ShadowViewGPU g = MakeGpuView(id, AtlasRect{ 1024, 2048, 512 }, 4096, 1.5f, 0.25f, 0.001f, true);
    CHECK(Near(g.atlasRect.x, 0.25f) && Near(g.atlasRect.y, 0.5f) && Near(g.atlasRect.z, 0.125f) && Near(g.atlasRect.w, 0.125f));
    CHECK(g.params.x == 1.5f && g.params.y == 0.25f && g.params.z == 0.001f && g.params.w == 1.0f);
    CHECK(MakeGpuView(id, AtlasRect{ 0, 0, 128 }, 4096, 1, 1, 0, false).params.w == 0.0f);
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "math: FloorPow2",                                        TestFloorPow2 },
        { "math: MulRows == XMVector4Transform (the shader's multiply)", TestMulRowsMatchesDirectXMath },
        { "cascades: split distances",                              TestCascadeSplits },
        { "cascades: contain their camera slice",                   TestCascadeContainsItsSlice },
        { "cascades: size ignores camera orientation (no shimmer)", TestCascadeSizeIgnoresCameraOrientation },
        { "cascades: snap to texels (no shimmer when walking)",     TestCascadeSnapsToTexels },
        { "cascades: cull box == the rendered volume",              TestCascadeCullBoxMatchesTheView },
        { "cascades: casters toward the light are included",        TestCascadeReachesCastersTowardTheLight },
        { "spot: view, frustum, extreme cone angles",               TestSpotView },
        { "point: six faces cover every direction + PCF border",    TestPointFacesCoverEveryDirection },
        { "atlas: packs exactly, never overlaps",                   TestAtlasPacksExactly },
        { "atlas: random mixed sizes never overlap",                TestAtlasMixedSizesNeverOverlap },
        { "atlas: rounding, minimum, reset, uninitialised",         TestAtlasRoundingAndReset },
        { "gpu view: atlas rect + params encoding",                 TestGpuViewEncoding },
    };
    for (auto& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "[ ok ]" : "[FAIL]", t.name);
    }
    std::printf("\n%s\n", g_failed == 0 ? "all shadow tests passed" : "SOME SHADOW TESTS FAILED");
    return g_failed == 0 ? 0 : 1;
}
