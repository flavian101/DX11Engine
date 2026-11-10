// Shadows without a GPU: the geometry of shadow maps (cascade fitting, spot / point views, the atlas allocator) and the
// planner that decides which lights get shadows. The important tests check PROPERTIES the pixel shader relies on:
// every pixel of the camera slice lands inside its cascade, a cascade does not shimmer when the camera moves, a point
// light's six faces cover every direction with room for the PCF kernel, the atlas never overlaps.
#include "renderer/Systems/LightPacking.h"
#include "renderer/Systems/LightClustering.h"
#include "renderer/Systems/SceneExtraction.h"
#include "renderer/Systems/ShadowMath.h"
#include "renderer/Systems/ShadowPlanner.h"
#include "scene/Scene.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <set>
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


// ======================================================================= the planner
static ExtractionCamera MakeCamera(XMFLOAT3 eye, XMFLOAT3 target, float farZ = 200.0f)
{
    ExtractionCamera c;
    XMStoreFloat4x4(&c.view, XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMVectorSet(0, 1, 0, 0)));
    XMStoreFloat4x4(&c.projection, XMMatrixPerspectiveFovLH(XMConvertToRadians(kFovDeg), kAspect, 0.1f, farZ));
    c.position = eye;
    return c;
}

static std::shared_ptr<Mesh> MakeCube(float size = 2.0f)
{
    return std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("cube", size)), nullptr);
}

static std::shared_ptr<Material> MakeMaterial(bool transparent = false)
{
    auto m = std::make_shared<Material>("m", transparent ? MaterialType::Transparent : MaterialType::Lit);
    if (transparent) m->SetBlendMode(RHI::BlendMode::AlphaBlend);
    return m;
}

static Entity AddCaster(Scene& scene, XMFLOAT3 pos, bool transparent = false, bool casts = true, bool visible = true)
{
    Entity e = scene.CreateEntity("caster");
    e.Get<TransformComponent>().position = pos;
    auto& r = e.Add<MeshRendererComponent>();
    r.mesh = MakeCube();
    r.materials = { MakeMaterial(transparent) };
    r.castsShadows = casts;
    r.visible = visible;
    return e;
}

static Entity AddSun(Scene& scene, XMFLOAT3 travel, float intensity = 3.0f, bool shadows = true)
{
    Entity e = scene.CreateEntity("sun");
    e.Get<TransformComponent>().SetForward(travel);
    auto& l = e.Add<LightComponent>();
    l.type = LightType::Directional; l.intensity = intensity; l.castsShadows = shadows;
    return e;
}

static Entity AddPoint(Scene& scene, XMFLOAT3 pos, float range = 15.0f, bool shadows = true)
{
    Entity e = scene.CreateEntity("point");
    e.Get<TransformComponent>().position = pos;
    auto& l = e.Add<LightComponent>();
    l.type = LightType::Point; l.range = range; l.castsShadows = shadows;
    return e;
}

static Entity AddSpot(Scene& scene, XMFLOAT3 pos, XMFLOAT3 dir, float range = 20.0f, bool shadows = true)
{
    Entity e = scene.CreateEntity("spot");
    e.Get<TransformComponent>().position = pos;
    e.Get<TransformComponent>().SetForward(dir);
    auto& l = e.Add<LightComponent>();
    l.type = LightType::Spot; l.range = range; l.outerConeDegrees = 35.0f; l.innerConeDegrees = 25.0f; l.castsShadows = shadows;
    return e;
}

static uint32_t Id(Entity e) { return static_cast<uint32_t>(entt::to_integral(e.Id())); }

struct Plan { ShadowFrame frame; RenderStatistics stats; };
static Plan RunPlanner(Scene& scene, const ExtractionCamera& cam, const ShadowSettings& settings)
{
    Plan p;
    TransformSystem::Update(scene);
    ShadowPlanner planner;
    planner.Plan(&scene, cam, settings, SceneExtraction(), p.frame, p.stats);
    return p;
}

static bool TilesAreValid(const ShadowFrame& f)
{
    for (size_t i = 0; i < f.views.size(); ++i)
    {
        const ShadowView& a = f.views[i];
        if (a.size == 0 || a.x + a.size > f.atlasSize || a.y + a.size > f.atlasSize) return false;
        for (size_t j = i + 1; j < f.views.size(); ++j)
        {
            const ShadowView& b = f.views[j];
            if (a.x < b.x + b.size && b.x < a.x + a.size && a.y < b.y + b.size && b.y < a.y + a.size) return false;
        }
        const XMFLOAT4& r = f.gpuViews[i].atlasRect;                      // the GPU copy describes the same tile
        const float inv = 1.0f / static_cast<float>(f.atlasSize);
        if (!Near(r.x, a.x * inv, 1e-6f) || !Near(r.y, a.y * inv, 1e-6f) || !Near(r.z, a.size * inv, 1e-6f)) return false;
    }
    return f.gpuViews.size() == f.views.size();
}

static void TestNothingWhenDisabledOrNotRequested()
{
    Scene scene;
    AddCaster(scene, { 0, 0, 10 });
    AddSun(scene, { 0.3f, -1, 0.2f }, 3.0f, /*shadows*/ false);               // a light that does not ask for shadows
    AddPoint(scene, { 0, 3, 8 }, 15.0f, false);
    const ExtractionCamera cam = MakeCamera({ 0, 2, 0 }, { 0, 0, 10 });

    Plan p = RunPlanner(scene, cam, ShadowSettings());
    CHECK(p.frame.views.empty() && p.frame.lights.empty() && p.frame.casters.empty());
    CHECK(p.stats.shadowViews == 0 && p.stats.shadowLightsDropped == 0);

    AddSun(scene, { 0.3f, -1, 0.2f });                                         // now one asks, but shadows are switched off globally
    ShadowSettings off; off.enabled = false;
    p = RunPlanner(scene, cam, off);
    CHECK(p.frame.views.empty() && p.frame.lights.empty());

    p = RunPlanner(scene, cam, ShadowSettings());                              // switched on: it appears
    CHECK(p.frame.views.size() == 4 && p.frame.lights.size() == 1);

    Plan none; ShadowPlanner planner;                                          // no scene at all: must not crash
    planner.Plan(nullptr, cam, ShadowSettings(), SceneExtraction(), none.frame, none.stats);
    CHECK(none.frame.views.empty());
}

static void TestDirectionalCascades()
{
    Scene scene;
    Entity sun = AddSun(scene, { 0.3f, -1, 0.2f });
    AddCaster(scene, { 0, 0, 10 });
    const ExtractionCamera cam = MakeCamera({ 0, 2, 0 }, { 0, 0, 10 }, 200.0f);
    ShadowSettings s; s.maxDistance = 100.0f;

    const Plan p = RunPlanner(scene, cam, s);
    CHECK(p.frame.views.size() == 4 && p.stats.shadowedDirectional == 1 && p.stats.shadowViews == 4);
    CHECK(p.frame.atlasSize == 4096 && TilesAreValid(p.frame));
    for (const ShadowView& v : p.frame.views) CHECK(v.kind == ShadowView::Kind::Cascade && v.size == 1024 && v.lightId == Id(sun));

    const auto it = p.frame.lights.find(Id(sun));
    CHECK(it != p.frame.lights.end());
    const ShadowLightAssignment& a = it->second;
    CHECK(a.firstView == 0 && a.viewCount == 4);
    CHECK(a.cascadeSplits.x < a.cascadeSplits.y && a.cascadeSplits.y < a.cascadeSplits.z && a.cascadeSplits.z < a.cascadeSplits.w);
    CHECK(Near(a.cascadeSplits.w, 100.0f));                                    // capped by maxDistance, not the 200 m far plane
    CHECK(a.fadeStart > a.cascadeSplits.z && a.fadeStart < a.cascadeSplits.w);

    // What the pixel shader does: pick the cascade from the view depth, project into it. Every visible point must land inside.
    std::mt19937 rng(21);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    std::uniform_real_distribution<float> depth(0.2f, 99.0f);
    for (int i = 0; i < 2000; ++i)
    {
        const float d = depth(rng);
        XMFLOAT3 world;
        const XMMATRIX inv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&cam.view));
        XMStoreFloat3(&world, XMVector3TransformCoord(XMVectorSet(u(rng) * TanX() * d, u(rng) * TanY() * d, d, 1), inv));

        const float splits[4] = { a.cascadeSplits.x, a.cascadeSplits.y, a.cascadeSplits.z, a.cascadeSplits.w };
        uint32_t cascade = 0;
        while (cascade < 3 && d > splits[cascade]) ++cascade;
        const XMFLOAT3 ndc = ToNdc(p.frame.gpuViews[a.firstView + cascade].viewProj, world);
        CHECK(std::fabs(ndc.x) <= 1.0f && std::fabs(ndc.y) <= 1.0f && ndc.z >= 0.0f && ndc.z <= 1.0f);
    }

    // settings are respected: 2 cascades of 512
    ShadowSettings two; two.cascadeCount = 2; two.cascadeResolution = 512; two.maxDistance = 50.0f;
    const Plan q = RunPlanner(scene, cam, two);
    CHECK(q.frame.views.size() == 2 && q.frame.views[0].size == 512 && q.frame.lights.begin()->second.viewCount == 2);
    CHECK(q.frame.lights.begin()->second.cascadeSplits.z == q.frame.lights.begin()->second.cascadeSplits.y);   // unused entries repeat
    CHECK(TilesAreValid(q.frame));
}

static void TestOnlyTheStrongestSunGetsCascades()
{
    Scene scene;
    Entity weak   = AddSun(scene, { 0, -1, 0.1f }, 1.0f);
    Entity strong = AddSun(scene, { 0.2f, -1, 0 }, 5.0f);
    const Plan p = RunPlanner(scene, MakeCamera({ 0, 2, 0 }, { 0, 0, 10 }), ShadowSettings());   // maxShadowedDirectional = 1
    CHECK(p.frame.lights.size() == 1 && p.frame.lights.count(Id(strong)) == 1 && p.frame.lights.count(Id(weak)) == 0);
    CHECK(p.stats.shadowedDirectional == 1 && p.stats.shadowLightsDropped == 1);

    ShadowSettings two; two.maxShadowedDirectional = 2;                                          // two suns: 8 cascades = 2 x 2048^2... still fits
    const Plan q = RunPlanner(scene, MakeCamera({ 0, 2, 0 }, { 0, 0, 10 }), two);
    CHECK(q.frame.lights.size() == 2 && q.frame.views.size() == 8 && TilesAreValid(q.frame));
}

static void TestLocalLightBudgetKeepsTheMostImportant()
{
    Scene scene;
    std::vector<Entity> lights;
    for (int i = 0; i < 12; ++i)                                               // in a row away from the camera: i = distance rank
        lights.push_back(AddPoint(scene, { 0, 1, 3.0f + 4.0f * i }, 40.0f));
    const ExtractionCamera cam = MakeCamera({ 0, 1, 0 }, { 0, 1, 10 });

    ShadowSettings s; s.maxShadowedLocalLights = 4;
    const Plan p = RunPlanner(scene, cam, s);
    CHECK(p.frame.lights.size() == 4 && p.stats.shadowedLocal == 4 && p.stats.shadowLightsDropped == 8);
    CHECK(p.frame.views.size() == 24);                                         // 4 point lights x 6 faces
    for (int i = 0; i < 4; ++i) CHECK(p.frame.lights.count(Id(lights[i])) == 1);    // the 4 nearest (= most important)
    for (int i = 4; i < 12; ++i) CHECK(p.frame.lights.count(Id(lights[i])) == 0);
    CHECK(TilesAreValid(p.frame));

    for (const auto& kv : p.frame.lights)                                      // six CONTIGUOUS faces per point light
    {
        CHECK(kv.second.viewCount == 6);
        for (uint32_t f = 0; f < 6; ++f)
        {
            CHECK(p.frame.views[kv.second.firstView + f].kind == ShadowView::Kind::PointFace);
            CHECK(p.frame.views[kv.second.firstView + f].lightId == kv.first);
        }
    }
}

static void TestLightsThatCannotBeSeenAreSkippedSilently()
{
    Scene scene;
    AddPoint(scene, { 0, 1, -100 }, 10.0f);                                    // far behind the camera, its reach never touches the view
    AddPoint(scene, { 900, 1, 20 }, 10.0f);                                    // far to the side
    const Plan p = RunPlanner(scene, MakeCamera({ 0, 1, 0 }, { 0, 1, 10 }), ShadowSettings());
    CHECK(p.frame.views.empty() && p.frame.lights.empty());
    CHECK(p.stats.shadowLightsDropped == 0);                                   // not "dropped": there was nothing to lose
}

static void TestSpotLightGetsOneView()
{
    Scene scene;
    Entity spot = AddSpot(scene, { 0, 6, 8 }, { 0, -1, 0 });
    const Plan p = RunPlanner(scene, MakeCamera({ 0, 1, 0 }, { 0, 1, 10 }), ShadowSettings());
    CHECK(p.frame.views.size() == 1 && p.frame.views[0].kind == ShadowView::Kind::Spot);
    CHECK(p.frame.lights.at(Id(spot)).firstView == 0 && p.frame.lights.at(Id(spot)).viewCount == 1);
    CHECK(p.frame.gpuViews[0].params.w == 1.0f);                               // perspective
    CHECK(TilesAreValid(p.frame));
}

static void TestAtlasExhaustionShrinksThenDrops()
{
    Scene scene;
    Entity point = AddPoint(scene, { 0, 1, 6 }, 20.0f);
    const ExtractionCamera cam = MakeCamera({ 0, 1, 0 }, { 0, 1, 10 });

    ShadowSettings s; s.atlasSize = 1024; s.pointFaceResolution = 512; s.minTileResolution = 128;   // 6 x 512^2 > 1024^2
    Plan p = RunPlanner(scene, cam, s);
    CHECK(p.frame.views.size() == 6 && p.stats.shadowLightsDropped == 0);
    for (const ShadowView& v : p.frame.views) CHECK(v.size == 256);                              // shrank to fit, all six together
    CHECK(TilesAreValid(p.frame));

    s.minTileResolution = 512;                                                                   // refuses to go below 512: dropped
    p = RunPlanner(scene, cam, s);
    CHECK(p.frame.views.empty() && p.frame.lights.count(Id(point)) == 0);                       // NO partial light: all or nothing
    CHECK(p.stats.shadowLightsDropped == 1 && p.stats.shadowedLocal == 0);
}

static void TestFailedAllocationDoesNotLeakTiles()
{
    Scene scene;
    AddSun(scene, { 0.3f, -1, 0.2f });
    AddSpot(scene, { 0, 6, 8 }, { 0, -1, 0 });
    const ExtractionCamera cam = MakeCamera({ 0, 1, 0 }, { 0, 1, 10 });

    ShadowSettings s; s.atlasSize = 1024; s.cascadeResolution = 512; s.minTileResolution = 512; s.spotResolution = 512;
    const Plan p = RunPlanner(scene, cam, s);                                  // the sun's 4 x 512 fill the 1024 atlas completely
    CHECK(p.frame.views.size() == 4 && p.frame.lights.size() == 1);
    CHECK(p.stats.shadowLightsDropped == 1);                                   // the spot did not fit
    CHECK(TilesAreValid(p.frame));
}

static void TestOrthographicCameraSkipsCascadesButNotLocalLights()
{
    Scene scene;
    AddSun(scene, { 0.3f, -1, 0.2f });
    AddPoint(scene, { 0, 1, 6 }, 20.0f);
    ExtractionCamera cam = MakeCamera({ 0, 1, 0 }, { 0, 1, 10 });
    XMStoreFloat4x4(&cam.projection, XMMatrixOrthographicLH(40, 20, 0.1f, 100.0f));
    const Plan p = RunPlanner(scene, cam, ShadowSettings());
    CHECK(p.frame.lights.size() == 1 && p.stats.shadowedDirectional == 0 && p.stats.shadowLightsDropped == 1);   // no cascades without a perspective frustum
}

static void TestCasterExtractionFilters()
{
    Scene scene;
    AddCaster(scene, { 0, 0, 10 });                                            // yes
    AddCaster(scene, { 0, 0, -90 });                                           // yes: behind the camera. Not culled by the VIEW.
    AddCaster(scene, { 3, 0, 10 }, false, /*casts*/ false);                    // castsShadows off
    AddCaster(scene, { 4, 0, 10 }, /*transparent*/ true);                      // glass does not cast
    AddCaster(scene, { 5, 0, 10 }, false, true, /*visible*/ false);            // hidden
    Entity sky = AddCaster(scene, { 6, 0, 10 }); sky.Add<SkyboxComponent>();   // the sky
    TransformSystem::Update(scene);

    std::vector<DrawItem> items; std::vector<DirectX::BoundingSphere> bounds;
    SceneExtraction().ExtractShadowCasters(scene, items, bounds);
    CHECK(items.size() == 2 && bounds.size() == 2);
    std::set<float> zs;
    for (size_t i = 0; i < items.size(); ++i)
    {
        zs.insert(items[i].world._43);
        CHECK(Near(bounds[i].Center.z, items[i].world._43, 1e-3f) && bounds[i].Radius > 1.0f);   // bounds travel with their item
    }
    CHECK(zs.count(10.0f) == 1 && zs.count(-90.0f) == 1);

    // the draw items of the NORMAL pass carry the receiver flag
    Scene s2;
    Entity cube = AddCaster(s2, { 0, 0, 10 });
    cube.Get<MeshRendererComponent>().receivesShadows = false;
    TransformSystem::Update(s2);
    BatchQueue q; RenderStatistics st;
    SceneExtraction().Extract(s2, MakeCamera({ 0, 0, 0 }, { 0, 0, 10 }), q, st);
    CHECK(q.opaque.size() == 1 && !q.opaque[0].receivesShadows);
}

static void TestCastersReachViewsThatTheyShadow()
{
    // Sun travelling forward and down; camera at the origin looking +Z.
    const XMFLOAT3 travel{ 0.0f, -0.6f, 0.8f };
    Scene scene;
    AddSun(scene, travel);
    const XMFLOAT3 inView{ 0, 0, 30 };                                         // a point in view
    XMFLOAT3 dir; XMStoreFloat3(&dir, XMVector3Normalize(XMLoadFloat3(&travel)));

    // a tower BEHIND the camera and far toward the light: invisible to the camera, but its shadow falls into the view
    const XMFLOAT3 tower{ inView.x - dir.x * 40.0f, inView.y - dir.y * 40.0f, inView.z - dir.z * 40.0f };
    const XMFLOAT3 downstream{ inView.x + dir.x * 300.0f, inView.y + dir.y * 300.0f, inView.z + dir.z * 300.0f };   // cannot shadow the view
    const XMFLOAT3 aside{ 400, 0, 30 };                                        // nowhere near
    AddCaster(scene, tower);
    AddCaster(scene, downstream);
    AddCaster(scene, aside);
    AddCaster(scene, inView);

    ShadowSettings s; s.maxDistance = 60.0f;
    const Plan p = RunPlanner(scene, MakeCamera({ 0, 2, 0 }, { 0, 2, 10 }), s);
    CHECK(p.frame.casters.size() == 4);

    auto reached = [&](const XMFLOAT3& pos)
    {
        for (const ShadowView& v : p.frame.views)
            for (uint32_t idx : v.casters)
                if (Near(p.frame.casters[idx].world._41, pos.x, 1e-3f) && Near(p.frame.casters[idx].world._42, pos.y, 1e-3f) &&
                    Near(p.frame.casters[idx].world._43, pos.z, 1e-3f))
                    return true;
        return false;
    };
    CHECK(reached(inView));
    CHECK(reached(tower));                                                     // the whole point of not culling by the camera
    CHECK(!reached(downstream));
    CHECK(!reached(aside));

    // every index stays inside the caster list
    for (const ShadowView& v : p.frame.views)
        for (uint32_t idx : v.casters) CHECK(idx < p.frame.casters.size());
}

static void TestPointFaceCastersAreCulledPerFace()
{
    Scene scene;
    AddPoint(scene, { 0, 1, 10 }, 8.0f);
    AddCaster(scene, { 5, 1, 10 });                                            // +X of the light, inside its range
    const Plan p = RunPlanner(scene, MakeCamera({ 0, 1, 0 }, { 0, 1, 10 }), ShadowSettings());
    CHECK(p.frame.views.size() == 6 && p.frame.casters.size() == 1);
    int facesThatSeeIt = 0;
    for (size_t f = 0; f < 6; ++f) facesThatSeeIt += p.frame.views[f].casters.empty() ? 0 : 1;
    CHECK(facesThatSeeIt >= 1 && facesThatSeeIt < 6);                          // the +X face sees it; the -X face must not
    CHECK(!p.frame.views[0].casters.empty());                                  // face 0 = +X
    CHECK(p.frame.views[1].casters.empty());                                   // face 1 = -X
}

static void TestLightBuffersPointAtTheRightViews()
{
    Scene scene;
    Entity sun   = AddSun(scene, { 0.3f, -1, 0.2f });
    Entity spot  = AddSpot(scene, { 4, 6, 10 }, { 0, -1, 0 });
    Entity point = AddPoint(scene, { -4, 3, 12 }, 15.0f);
    AddPoint(scene, { 0, 3, 14 }, 15.0f, /*shadows*/ false);                   // no shadow requested
    const Plan p = RunPlanner(scene, MakeCamera({ 0, 2, 0 }, { 0, 2, 10 }), ShadowSettings());

    SceneLightData data; std::vector<LightGPU> local;
    PackLights(&scene, RenderEnvironment(), { 0, 2, 0 }, data, local, kMaxLocalLights, &p.frame.lights);

    CHECK(data.directionalLightCount == 1);
    const DirectionalLightGPU& d = data.directionalLights[0];
    CHECK(d.shadowViewIndex == p.frame.lights.at(Id(sun)).firstView && Near(d.shadowParams.x, 4.0f));
    CHECK(d.cascadeSplits.w == p.frame.lights.at(Id(sun)).cascadeSplits.w && d.shadowParams.y == p.frame.lights.at(Id(sun)).fadeStart);

    int shadowed = 0, plain = 0;
    for (const LightGPU& l : local)
    {
        if (l.shadowIndex < 0) { ++plain; continue; }
        ++shadowed;
        const bool isSpot = l.type == GpuLight_Spot;
        const ShadowLightAssignment& a = p.frame.lights.at(isSpot ? Id(spot) : Id(point));
        CHECK(l.shadowIndex == a.firstView);

        // what the shader does for a fragment: find the view, project, and land INSIDE that view's tile of the atlas
        const XMFLOAT3 frag = isSpot ? XMFLOAT3{ 4, 0.5f, 10.5f } : XMFLOAT3{ -4 + 3.0f, 2.0f, 12 - 4.0f };
        uint32_t view = l.shadowIndex;
        if (!isSpot) view += PointFaceIndex({ frag.x - l.position.x, frag.y - l.position.y, frag.z - l.position.z });
        const XMFLOAT3 ndc = ToNdc(p.frame.gpuViews[view].viewProj, frag);
        CHECK(std::fabs(ndc.x) < 1.0f && std::fabs(ndc.y) < 1.0f && ndc.z > 0.0f && ndc.z < 1.0f);
        const XMFLOAT4& rect = p.frame.gpuViews[view].atlasRect;
        const float u = rect.x + (ndc.x * 0.5f + 0.5f) * rect.z, v = rect.y + (-ndc.y * 0.5f + 0.5f) * rect.w;
        CHECK(u >= rect.x && u <= rect.x + rect.z && v >= rect.y && v <= rect.y + rect.w && u > 0 && u < 1 && v > 0 && v < 1);
    }
    CHECK(shadowed == 2 && plain == 1);                                        // the light that did not ask got -1

    // without a table nothing is shadowed
    PackLights(&scene, RenderEnvironment(), { 0, 2, 0 }, data, local);
    CHECK(data.directionalLights[0].shadowViewIndex == -1);
    for (const LightGPU& l : local) CHECK(l.shadowIndex == -1);
}

static void TestSettingsAreSanitised()
{
    Scene scene;
    AddSun(scene, { 0.3f, -1, 0.2f });
    const ExtractionCamera cam = MakeCamera({ 0, 2, 0 }, { 0, 2, 10 });

    ShadowSettings silly;
    silly.cascadeCount = 99;                 // more than the shader supports
    silly.cascadeResolution = 1000;          // not a power of two
    silly.maxDistance = 0.0f;                // nothing to shadow
    Plan p = RunPlanner(scene, cam, silly);
    CHECK(p.frame.views.empty() && p.stats.shadowLightsDropped == 1);            // no range: dropped cleanly, no crash

    silly.maxDistance = 80.0f;
    p = RunPlanner(scene, cam, silly);
    CHECK(p.frame.views.size() == 4);                                            // clamped to 4
    for (const ShadowView& v : p.frame.views) CHECK(v.size == 512);              // 1000 -> 512

    ShadowSettings tiny; tiny.atlasSize = 0;
    p = RunPlanner(scene, cam, tiny);
    CHECK(p.frame.views.empty());
}

static void TestPlanningTwiceGivesTheSameResult()
{
    Scene scene;
    AddSun(scene, { 0.3f, -1, 0.2f });
    AddPoint(scene, { 0, 3, 10 }, 15.0f);
    AddSpot(scene, { 3, 6, 12 }, { 0, -1, 0 });
    AddCaster(scene, { 0, 0, 10 });
    const ExtractionCamera cam = MakeCamera({ 0, 2, 0 }, { 0, 2, 10 });
    TransformSystem::Update(scene);

    ShadowPlanner planner;                                                       // ONE planner reused: no state may leak between frames
    ShadowFrame a, b; RenderStatistics sa, sb;
    planner.Plan(&scene, cam, ShadowSettings(), SceneExtraction(), a, sa);
    planner.Plan(&scene, cam, ShadowSettings(), SceneExtraction(), b, sb);
    CHECK(a.views.size() == b.views.size() && a.views.size() == 11);
    for (size_t i = 0; i < a.views.size(); ++i)
    {
        CHECK(a.views[i].x == b.views[i].x && a.views[i].y == b.views[i].y && a.views[i].size == b.views[i].size);
        CHECK(a.views[i].casters == b.views[i].casters);
    }
    CHECK(sa.shadowViews == sb.shadowViews && sa.shadowedLocal == sb.shadowedLocal);
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
        { "planner: nothing when off / not requested",              TestNothingWhenDisabledOrNotRequested },
        { "planner: cascades, splits, every visible point inside",  TestDirectionalCascades },
        { "planner: only the strongest sun gets cascades",          TestOnlyTheStrongestSunGetsCascades },
        { "planner: local budget keeps the most important",         TestLocalLightBudgetKeepsTheMostImportant },
        { "planner: unseen lights skipped silently",                TestLightsThatCannotBeSeenAreSkippedSilently },
        { "planner: a spot light gets one view",                    TestSpotLightGetsOneView },
        { "planner: full atlas shrinks tiles, then drops (all or nothing)", TestAtlasExhaustionShrinksThenDrops },
        { "planner: a failed allocation leaks no tiles",            TestFailedAllocationDoesNotLeakTiles },
        { "planner: orthographic camera",                           TestOrthographicCameraSkipsCascadesButNotLocalLights },
        { "casters: extraction filters + receiver flag",            TestCasterExtractionFilters },
        { "casters: reach the views they shadow (not camera-culled)", TestCastersReachViewsThatTheyShadow },
        { "casters: point faces are culled per face",               TestPointFaceCastersAreCulledPerFace },
        { "lights: buffers point at the right views and tiles",     TestLightBuffersPointAtTheRightViews },
        { "planner: silly settings are sanitised",                  TestSettingsAreSanitised },
        { "planner: reused planner is deterministic",               TestPlanningTwiceGivesTheSameResult },
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
