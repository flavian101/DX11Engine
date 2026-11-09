// Clustered light assignment without a GPU. The important tests compare the fast path against its definition
// ("a light reaches a cluster when its sphere touches the cluster's box") on random lights and several cameras, and
// check that the formula the PIXEL SHADER uses to find its cluster lands inside the right box.
#include "renderer/Systems/LightClustering.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <set>
#include <utility>

using namespace DXEngine;
using namespace DirectX;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAILED line %d: %s\n", __LINE__, #cond); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

// ------------------------------------------------------------ helpers
static XMFLOAT4X4 Store(FXMMATRIX m) { XMFLOAT4X4 f; XMStoreFloat4x4(&f, m); return f; }
static XMFLOAT4X4 Identity() { return Store(XMMatrixIdentity()); }
static XMFLOAT4X4 Perspective(float fovDeg = 60, float aspect = 16.0f / 9.0f, float n = 0.5f, float f = 1000.0f)
{ return Store(XMMatrixPerspectiveFovLH(XMConvertToRadians(fovDeg), aspect, n, f)); }

static LightGPU MakeLight(float x, float y, float z, float range)
{
    LightGPU l{};
    l.position = { x, y, z }; l.range = range; l.color = { 1, 1, 1 }; l.type = GpuLight_Point; l.shadowIndex = -1;
    return l;
}

static constexpr uint32_t kW = 1920, kH = 1080;

// "which lights does every cluster have", as a set of (cluster, light) pairs
static std::set<std::pair<uint32_t, uint32_t>> Pairs(const LightClusterer& c)
{
    std::set<std::pair<uint32_t, uint32_t>> out;
    for (uint32_t i = 0; i < c.ClusterCount(); ++i)
        for (uint32_t k = 0; k < c.Ranges()[i].count; ++k)
            out.insert({ i, c.Indices()[c.Ranges()[i].offset + k] });
    return out;
}

// the definition: every cluster whose box the light's sphere touches
static std::set<std::pair<uint32_t, uint32_t>> BruteForce(const LightClusterer& c, const XMFLOAT4X4& view,
                                                          const std::vector<LightGPU>& lights)
{
    std::set<std::pair<uint32_t, uint32_t>> out;
    const XMMATRIX v = XMLoadFloat4x4(&view);
    for (uint32_t i = 0; i < lights.size(); ++i)
    {
        XMFLOAT3 p; XMStoreFloat3(&p, XMVector3TransformCoord(XMLoadFloat3(&lights[i].position), v));
        for (uint32_t cl = 0; cl < c.ClusterCount(); ++cl)
            if (SphereIntersectsAabb(p, lights[i].range, c.Bounds(cl)))
                out.insert({ cl, i });
    }
    return out;
}

// ------------------------------------------------------------ projection helpers
static void TestProjectionHelpers()
{
    const float cases[][2] = { { 0.5f, 1000.0f }, { 0.1f, 100.0f }, { 1.0f, 50.0f }, { 0.01f, 5000.0f } };
    for (auto& c : cases)
    {
        const XMFLOAT4X4 p = Perspective(60, 1.5f, c[0], c[1]);
        CHECK(ProjectionIsPerspective(p));
        CHECK(Near(ProjectionNearPlane(p) / c[0], 1.0f, 1e-3f));
        // far is recovered from 1 - _33 = n / (n - f): for a huge far / near ratio the float _33 is within one ulp
        // of 1 and the plane cannot be recovered precisely (harmless: only the shared slice maths depends on it).
        CHECK(Near(ProjectionFarPlane(p) / c[1], 1.0f, c[1] / c[0] > 10000.0f ? 0.1f : 1e-3f));
    }
    CHECK(!ProjectionIsPerspective(Store(XMMatrixOrthographicLH(20, 10, 0.1f, 100))));
    CHECK(!ProjectionIsPerspective(Store(XMMatrixIdentity())));
}

// ------------------------------------------------------------ depth slices
static void TestDepthSlicesCoverNearToFar()
{
    LightClusterer c;
    CHECK(c.SetCamera(Identity(), Perspective(), kW, kH));
    const uint32_t n = c.Desc().slicesZ;

    CHECK(c.SliceOf(c.NearPlane()) == 0);
    CHECK(c.SliceOf(c.FarPlane() * 0.9999f) == n - 1);
    CHECK(c.SliceOf(c.FarPlane() * 10.0f) == n - 1);            // beyond far: clamped
    CHECK(c.SliceOf(0.0f) == 0 && c.SliceOf(-5.0f) == 0);       // at / behind the camera: clamped

    uint32_t last = 0;                                          // never goes backwards
    for (int i = 0; i <= 2000; ++i)
    {
        const float z = c.NearPlane() * std::pow(c.FarPlane() / c.NearPlane(), i / 2000.0f);
        const uint32_t s = c.SliceOf(z);
        CHECK(s >= last);
        last = s;
    }

    // the boxes of one column tile the depth range without gaps, and each slice's middle maps back to itself
    CHECK(Near(c.Bounds(c.ClusterIndex(0, 0, 0)).min.z, c.NearPlane(), 1e-3f));
    for (uint32_t z = 0; z < n; ++z)
    {
        const ClusterAabb& b = c.Bounds(c.ClusterIndex(3, 2, z));
        if (z + 1 < n) CHECK(Near(b.max.z, c.Bounds(c.ClusterIndex(3, 2, z + 1)).min.z, 1e-3f * b.max.z));
        CHECK(c.SliceOf(0.5f * (b.min.z + b.max.z)) == z);
    }
    CHECK(Near(c.Bounds(c.ClusterIndex(0, 0, n - 1)).max.z / c.FarPlane(), 1.0f, 1e-3f));
}

// ------------------------------------------------------------ the pixel shader's lookup
// Mirrors ClusterIndexOf() in common.hlsli, built only from Constants(): if this lands in the wrong box, the shader would too.
static uint32_t ShaderClusterOf(const ClusterConstants& k, float pixelX, float pixelY, float viewZ)
{
    const uint32_t tx = std::min(static_cast<uint32_t>(pixelX * k.tilesPerPixelX), k.gridX - 1);
    const uint32_t ty = std::min(static_cast<uint32_t>(pixelY * k.tilesPerPixelY), k.gridY - 1);
    int slice = static_cast<int>(std::floor(std::log(viewZ) * k.sliceScale + k.sliceBias));
    slice = std::max(0, std::min(slice, static_cast<int>(k.gridZ) - 1));
    return tx + ty * k.gridX + static_cast<uint32_t>(slice) * k.gridX * k.gridY;
}

static void RunShaderLookupTest(const XMFLOAT4X4& proj)
{
    LightClusterer c;
    CHECK(c.SetCamera(Identity(), proj, kW, kH));
    const ClusterConstants k = c.Constants(1);
    CHECK(k.mode == 1 && k.gridX == 16 && k.gridY == 9 && k.gridZ == 24);

    std::mt19937 rng(7);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    const XMMATRIX P = XMLoadFloat4x4(&proj);
    int tested = 0;
    const int failedBefore = g_failed;
    for (int i = 0; i < 20000; ++i)
    {
        const float z = c.NearPlane() * 1.001f * std::pow(c.FarPlane() * 0.998f / (c.NearPlane() * 1.001f), u(rng));
        const XMFLOAT3 v{ (u(rng) * 4 - 2) * z, (u(rng) * 4 - 2) * z, z };      // a random view-space point
        XMFLOAT3 ndc; XMStoreFloat3(&ndc, XMVector3TransformCoord(XMLoadFloat3(&v), P));   // the real projection
        if (std::fabs(ndc.x) >= 1.0f || std::fabs(ndc.y) >= 1.0f) continue;                // off screen
        ++tested;

        const float px = (ndc.x * 0.5f + 0.5f) * kW, py = (0.5f - ndc.y * 0.5f) * kH;       // where the pixel is
        const ClusterAabb& b = c.Bounds(ShaderClusterOf(k, px, py, z));
        const float e = 2e-3f * (1.0f + z);
        CHECK(v.x >= b.min.x - e && v.x <= b.max.x + e && v.y >= b.min.y - e && v.y <= b.max.y + e &&
              v.z >= b.min.z - e && v.z <= b.max.z + e);
        if (g_failed != failedBefore) break;
    }
    CHECK(tested > 2000);                                                                  // the test really ran
}

static void TestShaderLookupLandsInsideTheCluster()
{
    RunShaderLookupTest(Perspective());                                                    // centred
    RunShaderLookupTest(Store(XMMatrixPerspectiveOffCenterLH(-0.4f, 0.9f, -0.3f, 0.35f, 0.5f, 1000.0f)));   // off-centre
}

// ------------------------------------------------------------ assignment
static void TestSmallLightReachesItsCluster()
{
    LightClusterer c;
    c.SetCamera(Identity(), Perspective(), kW, kH);
    const LightGPU l = MakeLight(3.0f, 1.0f, 20.0f, 0.05f);
    c.Assign(&l, 1);

    // where a pixel at the light's position would look
    const XMFLOAT4X4 proj = Perspective();
    const XMMATRIX P = XMLoadFloat4x4(&proj);
    XMFLOAT3 ndc; XMStoreFloat3(&ndc, XMVector3TransformCoord(XMVectorSet(3, 1, 20, 1), P));
    const uint32_t cluster = ShaderClusterOf(c.Constants(1), (ndc.x * 0.5f + 0.5f) * kW, (0.5f - ndc.y * 0.5f) * kH, 20.0f);

    const ClusterRange r = c.Ranges()[cluster];
    CHECK(r.count == 1 && c.Indices()[r.offset] == 0);
    CHECK(c.Stats().lightsInFrustum == 1 && c.Stats().lightsCulled == 0);
    CHECK(c.Stats().assignments >= 1 && c.Stats().assignments <= 8);                       // 1 cluster, a few at a corner
}

static void TestLightsOutsideTheFrustumAreCulled()
{
    LightClusterer c;
    c.SetCamera(Identity(), Perspective(), kW, kH);
    const std::vector<LightGPU> lights = {
        MakeLight(0, 0, -10, 5),        // behind the camera
        MakeLight(0, 0, 2000, 10),      // beyond far
        MakeLight(-500, 0, 10, 5),      // far to the left of the screen
        MakeLight(0, 800, 10, 5),       // far above
        MakeLight(0, 0, 10, 0.0f),      // no range
    };
    c.Assign(lights.data(), static_cast<uint32_t>(lights.size()));
    CHECK(c.Stats().lightsCulled == 5 && c.Stats().lightsInFrustum == 0);
    CHECK(c.IndexCount() == 0 && c.Stats().assignments == 0);
}

static void TestNearPlaneAndCameraInsideALight()
{
    LightClusterer c;
    c.SetCamera(Identity(), Perspective(), kW, kH);

    const LightGPU nearLight = MakeLight(0.0f, 0.0f, 0.1f, 1.0f);    // straddles the near plane (0.5)
    c.Assign(&nearLight, 1);
    bool inFirstSlice = false;
    for (uint32_t i = 0; i < c.Desc().tilesX * c.Desc().tilesY; ++i) inFirstSlice |= c.Ranges()[i].count > 0;
    CHECK(inFirstSlice);

    const LightGPU huge = MakeLight(0, 0, 0, 5000.0f);               // the camera is inside it, it covers everything
    c.Assign(&huge, 1);
    CHECK(c.Stats().assignments == c.ClusterCount());
    for (const ClusterRange& r : c.Ranges()) CHECK(r.count == 1);
}

// The cluster a visible point falls into, found the way the pixel shader finds it (real projection -> pixel -> tile / slice).
static bool ClusterOfViewPoint(const LightClusterer& c, const XMFLOAT4X4& proj, const XMFLOAT3& v, uint32_t& cluster)
{
    if (!(v.z > c.NearPlane() * 1.0005f && v.z < c.FarPlane() * 0.9995f)) return false;
    const XMMATRIX P = XMLoadFloat4x4(&proj);
    XMFLOAT3 ndc; XMStoreFloat3(&ndc, XMVector3TransformCoord(XMLoadFloat3(&v), P));
    if (std::fabs(ndc.x) >= 0.9995f || std::fabs(ndc.y) >= 0.9995f) return false;
    cluster = ShaderClusterOf(c.Constants(1), (ndc.x * 0.5f + 0.5f) * kW, (0.5f - ndc.y * 0.5f) * kH, v.z);
    return true;
}

// Two properties, on random lights:
//   1. COMPLETE: every point that a light reaches and that is visible lies in a cluster whose list has that light.
//      (If this failed, a lit pixel would be missing a light: visible as a dark patch.)
//   2. NOT WILDER than the definition: nothing is assigned to a cluster whose box the sphere does not touch.
//      (It is allowed to be tighter: a cluster's box is bigger than its frustum cell, and the sliver outside the
//      frustum cannot be seen.)
static void RunClusterProperties(const XMFLOAT4X4& view, const XMFLOAT4X4& proj, uint32_t seed)
{
    ClusterGridDesc desc; desc.maxLightIndices = 1u << 22;            // this test is about WHICH clusters, not about overflow
    LightClusterer c(desc);
    CHECK(c.SetCamera(view, proj, kW, kH));

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> pos(-90.0f, 90.0f), rad(0.2f, 25.0f), unit(-1.0f, 1.0f);
    // Three quarters of the lights are placed inside the visible frustum (so every camera sees plenty of them),
    // the rest anywhere (so culling is exercised too).
    const XMMATRIX viewInverse = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view));
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);
    std::vector<LightGPU> lights;
    for (int i = 0; i < 400; ++i)
    {
        if (i % 4 == 3) { lights.push_back(MakeLight(pos(rng), pos(rng), pos(rng), rad(rng))); continue; }
        const float z = 1.0f * std::pow(120.0f, u01(rng));                                // 1 .. 120, log-uniform
        const XMFLOAT3 inView{ (unit(rng) - proj._31) * z / proj._11, (unit(rng) - proj._32) * z / proj._22, z };
        XMFLOAT3 world; XMStoreFloat3(&world, XMVector3TransformCoord(XMLoadFloat3(&inView), viewInverse));
        lights.push_back(MakeLight(world.x, world.y, world.z, rad(rng)));
    }

    c.Assign(lights.data(), static_cast<uint32_t>(lights.size()));
    CHECK(c.Stats().assignmentsDropped == 0);
    const auto fast = Pairs(c);

    // 2. subset of the definition (and ascending, duplicate-free lists)
    const auto reference = BruteForce(c, view, lights);
    CHECK(std::includes(reference.begin(), reference.end(), fast.begin(), fast.end()));
    CHECK(fast.size() > 500);
    for (uint32_t cl = 0; cl < c.ClusterCount(); ++cl)
        for (uint32_t k = 1; k < c.Ranges()[cl].count; ++k)
            CHECK(c.Indices()[c.Ranges()[cl].offset + k - 1] < c.Indices()[c.Ranges()[cl].offset + k]);

    // 1. completeness, by sampling points inside every sphere
    const XMMATRIX V = XMLoadFloat4x4(&view);
    int visibleSamples = 0, missed = 0;
    for (uint32_t i = 0; i < lights.size(); ++i)
    {
        for (int s = 0; s < 150; ++s)
        {
            XMFLOAT3 d{ unit(rng), unit(rng), unit(rng) };
            if (d.x * d.x + d.y * d.y + d.z * d.z > 1.0f) continue;                      // inside the unit sphere
            const float r = lights[i].range * 0.999f;
            XMFLOAT3 world{ lights[i].position.x + d.x * r, lights[i].position.y + d.y * r, lights[i].position.z + d.z * r };
            XMFLOAT3 v; XMStoreFloat3(&v, XMVector3TransformCoord(XMLoadFloat3(&world), V));

            uint32_t cluster;
            if (!ClusterOfViewPoint(c, proj, v, cluster)) continue;                      // not visible
            ++visibleSamples;
            if (fast.count({ cluster, i }) == 0) ++missed;
        }
    }
    CHECK(visibleSamples > 3000);                                                        // the test really sampled the screen
    CHECK(missed == 0);
}

static void TestClusterAssignmentProperties()
{
    RunClusterProperties(Identity(), Perspective(), 1);
    RunClusterProperties(Store(XMMatrixLookToLH(XMVectorSet(5, 8, -20, 1), XMVectorSet(0.3f, -0.2f, 1, 0), XMVectorSet(0, 1, 0, 0))),
                         Perspective(90, 1.0f, 0.3f, 400.0f), 2);
    RunClusterProperties(Store(XMMatrixLookToLH(XMVectorSet(-30, 2, 10, 1), XMVectorSet(1, 0, 0.2f, 0), XMVectorSet(0, 1, 0, 0))),
                         Perspective(20, 21.0f / 9.0f, 0.1f, 200.0f), 3);
    RunClusterProperties(Identity(), Store(XMMatrixPerspectiveOffCenterLH(-0.4f, 0.9f, -0.3f, 0.35f, 0.5f, 1000.0f)), 4);
}

static void TestIndexListOverflowIsGraceful()
{
    ClusterGridDesc d; d.maxLightIndices = 64;
    LightClusterer c(d);
    c.SetCamera(Identity(), Perspective(), kW, kH);

    std::vector<LightGPU> lights;
    for (int i = 0; i < 100; ++i) lights.push_back(MakeLight(0.01f * i, 0, 10, 3.0f));   // 100 lights on the same spot
    c.Assign(lights.data(), 100);

    CHECK(c.Stats().assignmentsDropped > 0);
    CHECK(c.IndexCount() == 64);                                                         // exactly full, not beyond
    CHECK(c.Stats().assignments - c.Stats().assignmentsDropped == 64);

    uint32_t expectedOffset = 0;
    for (const ClusterRange& r : c.Ranges())                                             // contiguous, inside the list
    {
        CHECK(r.offset == expectedOffset);
        expectedOffset += r.count;
        for (uint32_t k = 0; k < r.count; ++k) CHECK(c.Indices()[r.offset + k] < 100);
    }
    CHECK(expectedOffset == 64);
}

// ------------------------------------------------------------ housekeeping
static void TestSteadyStateReusesItsBuffers()
{
    LightClusterer c;
    c.SetCamera(Identity(), Perspective(), kW, kH);
    std::vector<LightGPU> lights;
    for (int i = 0; i < 200; ++i) lights.push_back(MakeLight(float(i % 20) - 10, float(i % 7), 10.0f + i % 30, 4.0f));

    c.Assign(lights.data(), 200);
    const ClusterRange* ranges = c.Ranges().data(); const uint32_t* indices = c.Indices();
    c.Assign(lights.data(), 50);
    c.Assign(lights.data(), 200);
    c.Assign(nullptr, 0);
    CHECK(c.Ranges().data() == ranges && c.Indices() == indices);                        // nothing was reallocated
    CHECK(c.IndexCount() == 0 && c.Ranges().size() == c.ClusterCount());                 // and nothing stale is left
}

static void TestGridIsRebuiltOnlyWhenProjectionOrScreenChange()
{
    LightClusterer c;
    c.SetCamera(Identity(), Perspective(), kW, kH);
    const ClusterAabb before = c.Bounds(100);

    c.SetCamera(Store(XMMatrixTranslation(5, 0, 5)), Perspective(), kW, kH);             // the camera moved: same boxes
    CHECK(c.Bounds(100).min.x == before.min.x && c.Bounds(100).max.z == before.max.z);

    c.SetCamera(Identity(), Perspective(90), kW, kH);                                    // new fov: boxes change
    CHECK(c.Bounds(100).max.x != before.max.x);
    const ClusterAabb wide = c.Bounds(100);
    c.SetCamera(Identity(), Perspective(90), kW / 2, kH / 2);                            // resize: tilesPerPixel changes, boxes don't
    CHECK(c.Bounds(100).max.x == wide.max.x);
    CHECK(Near(c.Constants(1).tilesPerPixelX, 16.0f / (kW / 2), 1e-6f));
}

static void TestUnusableCamerasFallBackToLoopingOverAllLights()
{
    LightClusterer c;
    CHECK(!c.SetCamera(Identity(), Store(XMMatrixOrthographicLH(40, 22, 0.1f, 100)), kW, kH));
    const LightGPU l = MakeLight(0, 0, 10, 5);
    c.Assign(&l, 1);
    CHECK(c.IndexCount() == 0 && c.Constants(1).mode == 0 && c.Constants(2).mode == 0);   // the shader is told "no clusters"

    CHECK(!c.SetCamera(Identity(), Perspective(), 0, 0));                                 // empty screen
    CHECK(c.Constants(1).mode == 0);

    CHECK(c.SetCamera(Identity(), Perspective(), kW, kH));                                // and it recovers
    CHECK(c.Constants(2).mode == 2 && c.Constants(0).mode == 0);
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "clusters: projection helpers",                              TestProjectionHelpers },
        { "clusters: depth slices cover near..far",                    TestDepthSlicesCoverNearToFar },
        { "clusters: the shader's lookup lands in the right box",      TestShaderLookupLandsInsideTheCluster },
        { "clusters: a small light reaches its cluster",               TestSmallLightReachesItsCluster },
        { "clusters: lights outside the frustum are culled",           TestLightsOutsideTheFrustumAreCulled },
        { "clusters: near plane, camera inside a light",               TestNearPlaneAndCameraInsideALight },
        { "clusters: complete + within the definition (4 cameras)",    TestClusterAssignmentProperties },
        { "clusters: index list overflow is graceful",                 TestIndexListOverflowIsGraceful },
        { "clusters: steady state reuses its buffers",                 TestSteadyStateReusesItsBuffers },
        { "clusters: grid rebuilt only for projection / screen",       TestGridIsRebuiltOnlyWhenProjectionOrScreenChange },
        { "clusters: unusable cameras fall back to all lights",        TestUnusableCamerasFallBackToLoopingOverAllLights },
    };
    for (auto& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "[ ok ]" : "[FAIL]", t.name);
    }
    std::printf("\n%s\n", g_failed == 0 ? "all cluster tests passed" : "SOME CLUSTER TESTS FAILED");
    return g_failed == 0 ? 0 : 1;
}
