// Light packing without a GPU: Scene LightComponents -> SceneLightData (environment + directional lights)
// and the LightGPU list that becomes the structured buffer of point + spot lights.
#include "renderer/Systems/LightPacking.h"
#include "scene/Scene.h"
#include "scene/TransformSystem.h"
#include <cmath>
#include <cstdio>

using namespace DXEngine;
using namespace DirectX;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAILED line %d: %s\n", __LINE__, #cond); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

static Entity AddPoint(Scene& scene, XMFLOAT3 pos, float range = 10.0f, float intensity = 1.0f)
{
    Entity e = scene.CreateEntity("point");
    e.Get<TransformComponent>().position = pos;
    auto& l = e.Add<LightComponent>(); l.type = LightType::Point; l.range = range; l.intensity = intensity;
    return e;
}

// ------------------------------------------------------------ environment + directional
static void TestEnvironmentOnly()
{
    RenderEnvironment env; env.exposure = 0.7f; env.gamma = 2.0f; env.ambientIntensity = 0.25f;
    SceneLightData data; std::vector<LightGPU> local;
    CHECK(PackLights(nullptr, env, { 0, 0, 0 }, data, local) == 0);
    CHECK(data.directionalLightCount == 0 && data.localLightCount == 0 && local.empty());
    CHECK(Near(data.exposure, 0.7f) && Near(data.gamma, 2.0f) && Near(data.ambientIntensity, 0.25f));
}

static void TestDirectionalAimsAlongForward()
{
    const XMFLOAT3 dirs[] = { { 1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0.3f, -0.5f, 0.8f }, { 0, 0, -1 } };
    for (auto d : dirs)
    {
        Scene scene;
        Entity sun = scene.CreateEntity("sun");
        sun.Get<TransformComponent>().SetForward(d);
        auto& l = sun.Add<LightComponent>(); l.type = LightType::Directional; l.intensity = 3.0f;
        TransformSystem::Update(scene);

        SceneLightData data; std::vector<LightGPU> local;
        PackLights(&scene, RenderEnvironment(), { 0, 0, 0 }, data, local);
        const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        CHECK(data.directionalLightCount == 1 && local.empty());     // a sun never goes to the structured buffer
        CHECK(Near(data.directionalLights[0].direction.x, d.x / len) && Near(data.directionalLights[0].direction.y, d.y / len) &&
              Near(data.directionalLights[0].direction.z, d.z / len));
        CHECK(Near(data.directionalLights[0].intensity, 3.0f));
    }
}

static void TestDirectionalLightsAreCappedAtFour()
{
    Scene scene;
    for (int i = 0; i < 10; ++i)
    {
        Entity e = scene.CreateEntity("sun");
        e.Add<LightComponent>().type = LightType::Directional;
    }
    TransformSystem::Update(scene);
    SceneLightData data; std::vector<LightGPU> local;
    PackLights(&scene, RenderEnvironment(), { 0, 0, 0 }, data, local);
    CHECK(data.directionalLightCount == SceneLightData::MAX_DIRECTIONAL_LIGHTS);
}

// ------------------------------------------------------------ local lights
static void TestPointAndSpotConversion()
{
    Scene scene;
    Entity holder = scene.CreateEntity("holder");
    holder.Get<TransformComponent>().position = { 10, 0, 0 };
    Entity lamp = scene.CreateEntity("lamp", holder);                     // light moves with its parent
    lamp.Get<TransformComponent>().position = { 0, 5, 0 };
    auto& p = lamp.Add<LightComponent>(); p.type = LightType::Point; p.color = { 1, 0.5f, 0 }; p.intensity = 7; p.range = 25;

    Entity spot = scene.CreateEntity("spot");
    spot.Get<TransformComponent>().SetForward({ 0, -1, 0 });
    auto& s = spot.Add<LightComponent>(); s.type = LightType::Spot; s.innerConeDegrees = 10; s.outerConeDegrees = 30; s.range = 12;

    TransformSystem::Update(scene);
    SceneLightData data; std::vector<LightGPU> local;
    PackLights(&scene, RenderEnvironment(), { 0, 0, 0 }, data, local);

    CHECK(data.localLightCount == 2 && local.size() == 2 && data.directionalLightCount == 0);

    const LightGPU* point = nullptr; const LightGPU* sp = nullptr;
    for (const auto& l : local) (l.type == GpuLight_Point ? point : sp) = &l;
    CHECK(point && sp);
    if (!point || !sp) return;

    CHECK(Near(point->position.x, 10) && Near(point->position.y, 5));     // world position through the parent
    CHECK(Near(point->range, 25));
    CHECK(Near(point->color.x, 7) && Near(point->color.y, 3.5f) && Near(point->color.z, 0));   // colour * intensity
    CHECK(point->shadowIndex == -1);

    CHECK(sp->type == GpuLight_Spot && Near(sp->range, 12));
    CHECK(Near(sp->direction.y, -1));
    // the cone is saturate(cos * scale + offset): 0 at the outer cone, 1 at the inner cone, linear in between
    const float cosInner = std::cos(XMConvertToRadians(10)), cosOuter = std::cos(XMConvertToRadians(30));
    CHECK(Near(cosOuter * sp->spotScale + sp->spotOffset, 0.0f));
    CHECK(Near(cosInner * sp->spotScale + sp->spotOffset, 1.0f));
    CHECK(Near(0.5f * (cosInner + cosOuter) * sp->spotScale + sp->spotOffset, 0.5f));
}

static void TestHardEdgedSpotStaysFinite()
{
    Scene scene;                                                         // inner == outer: the old shader divided by zero
    Entity e = scene.CreateEntity("spot");
    auto& s = e.Add<LightComponent>(); s.type = LightType::Spot; s.innerConeDegrees = 25; s.outerConeDegrees = 25; s.range = 5;
    Entity f = scene.CreateEntity("spot2");                              // inner wider than outer: clamped, not inverted
    auto& s2 = f.Add<LightComponent>(); s2.type = LightType::Spot; s2.innerConeDegrees = 40; s2.outerConeDegrees = 20; s2.range = 5;
    TransformSystem::Update(scene);

    SceneLightData data; std::vector<LightGPU> local;
    PackLights(&scene, RenderEnvironment(), { 0, 0, 0 }, data, local);
    CHECK(local.size() == 2);
    for (const auto& l : local)
        CHECK(std::isfinite(l.spotScale) && std::isfinite(l.spotOffset) && l.spotScale > 0.0f);
}

static void TestSwitchedOffAndRangelessLightsAreSkipped()
{
    Scene scene;
    AddPoint(scene, { 1, 0, 0 });                                         // the only one that counts
    AddPoint(scene, { 2, 0, 0 }, 10.0f, 0.0f);                            // intensity 0
    AddPoint(scene, { 3, 0, 0 }, 0.0f, 1.0f);                             // no range
    AddPoint(scene, { 4, 0, 0 }, 10.0f, -1.0f);                           // negative intensity
    Entity sun = scene.CreateEntity("sun");                               // range is irrelevant for a directional light
    auto& l = sun.Add<LightComponent>(); l.type = LightType::Directional; l.range = 0.0f;
    TransformSystem::Update(scene);

    SceneLightData data; std::vector<LightGPU> local;
    PackLights(&scene, RenderEnvironment(), { 0, 0, 0 }, data, local);
    CHECK(local.size() == 1 && Near(local[0].position.x, 1));
    CHECK(data.directionalLightCount == 1);
}

static void TestManyLightsAreNotCapped()
{
    Scene scene;                                                          // the old limit was 64 point + 32 spot
    for (int i = 0; i < 1000; ++i) AddPoint(scene, { float(i), 0, 0 });
    TransformSystem::Update(scene);

    SceneLightData data; std::vector<LightGPU> local;
    CHECK(PackLights(&scene, RenderEnvironment(), { 0, 0, 0 }, data, local) == 0);
    CHECK(data.localLightCount == 1000 && local.size() == 1000);
}

static void TestOverTheLimitKeepsTheNearest()
{
    Scene scene;
    for (int i = 0; i < 100; ++i) AddPoint(scene, { float(i), 0, 0 });    // x = 0..99, camera at the origin
    TransformSystem::Update(scene);

    SceneLightData data; std::vector<LightGPU> local;
    const uint32_t dropped = PackLights(&scene, RenderEnvironment(), { 0, 0, 0 }, data, local, 64);
    CHECK(dropped == 36 && data.localLightCount == 64 && local.size() == 64);
    float maxX = 0; for (const auto& l : local) maxX = std::fmax(maxX, l.position.x);
    CHECK(Near(maxX, 63));                                                // exactly the 64 nearest, x = 0..63

    // the camera decides what is near
    PackLights(&scene, RenderEnvironment(), { 99, 0, 0 }, data, local, 10);
    float minX = 1000; for (const auto& l : local) minX = std::fmin(minX, l.position.x);
    CHECK(local.size() == 10 && Near(minX, 90));
}

static void TestBuffersAreReusedAcrossFrames()
{
    Scene scene;
    for (int i = 0; i < 50; ++i) AddPoint(scene, { float(i), 0, 0 });
    TransformSystem::Update(scene);

    SceneLightData data; std::vector<LightGPU> local;
    PackLights(&scene, RenderEnvironment(), { 0, 0, 0 }, data, local);
    const size_t capacity = local.capacity();
    const LightGPU* storage = local.data();
    PackLights(&scene, RenderEnvironment(), { 0, 0, 0 }, data, local);
    CHECK(local.capacity() == capacity && local.data() == storage);       // a steady scene allocates nothing

    PackLights(nullptr, RenderEnvironment(), { 0, 0, 0 }, data, local);   // and nothing stale is left behind
    CHECK(local.empty() && data.localLightCount == 0);
}

static void TestBufferCapacityGrowsInDoublingSteps()
{
    CHECK(LightBufferCapacityFor(0) == kMinLocalLightCapacity);
    CHECK(LightBufferCapacityFor(kMinLocalLightCapacity) == kMinLocalLightCapacity);
    CHECK(LightBufferCapacityFor(kMinLocalLightCapacity + 1) == 2 * kMinLocalLightCapacity);
    CHECK(LightBufferCapacityFor(1000) == 1024);
    CHECK(LightBufferCapacityFor(5000) == 8192);
    CHECK(LightBufferCapacityFor(kMaxLocalLights) == kMaxLocalLights);
    CHECK(LightBufferCapacityFor(kMaxLocalLights + 5000) == kMaxLocalLights);   // never beyond the limit
    for (uint32_t n : { 1u, 255u, 256u, 257u, 3000u, 8192u })
        CHECK(LightBufferCapacityFor(n) >= n);                                  // always big enough (below the limit)
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "lights: environment only",                              TestEnvironmentOnly },
        { "lights: directional aims along forward",                TestDirectionalAimsAlongForward },
        { "lights: directional lights capped at four",             TestDirectionalLightsAreCappedAtFour },
        { "lights: point/spot conversion through hierarchy",       TestPointAndSpotConversion },
        { "lights: hard-edged spot cone stays finite",             TestHardEdgedSpotStaysFinite },
        { "lights: switched-off and rangeless lights skipped",     TestSwitchedOffAndRangelessLightsAreSkipped },
        { "lights: no 64/32 cap any more",                         TestManyLightsAreNotCapped },
        { "lights: over the limit keeps the nearest",              TestOverTheLimitKeepsTheNearest },
        { "lights: output buffers reused across frames",           TestBuffersAreReusedAcrossFrames },
        { "lights: GPU buffer capacity doubles up to the limit",   TestBufferCapacityGrowsInDoublingSteps },
    };
    for (auto& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "[ ok ]" : "[FAIL]", t.name);
    }
    std::printf("\n%s\n", g_failed == 0 ? "all light tests passed" : "SOME LIGHT TESTS FAILED");
    return g_failed == 0 ? 0 : 1;
}
