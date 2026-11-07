// Scene core tests. No GPU, no window: plain asserts so they run anywhere.
//   build: see DXEngine/tests/CMakeLists.txt  (cmake -DDXENGINE_BUILD_TESTS=ON)
#include "scene/Scene.h"
#include "scene/TransformSystem.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace DXEngine;
using namespace DirectX;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAILED line %d: %s\n", __LINE__, #cond); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

static void TestWorldFromLocal()
{
    Scene scene;
    Entity e = scene.CreateEntity("box");
    auto& t = e.Get<TransformComponent>();
    t.position = { 1, 2, 3 }; t.scale = { 2, 2, 2 };
    TransformSystem::Update(scene);
    CHECK(Near(t.WorldPosition().x, 1) && Near(t.WorldPosition().y, 2) && Near(t.WorldPosition().z, 3));
    // a point at local (1,0,0) ends up at (1 + 2*1, 2, 3)
    XMFLOAT3 p; XMStoreFloat3(&p, XMVector3Transform(XMVectorSet(1, 0, 0, 1), t.World()));
    CHECK(Near(p.x, 3) && Near(p.y, 2) && Near(p.z, 3));
}

static void TestParentChildComposition()
{
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    Entity child  = scene.CreateEntity("child", parent);
    parent.Get<TransformComponent>().position = { 10, 0, 0 };
    parent.Get<TransformComponent>().scale = { 2, 2, 2 };
    child.Get<TransformComponent>().position = { 1, 0, 0 };
    TransformSystem::Update(scene);
    // child local (1,0,0) is scaled by 2 and moved by the parent: world x = 10 + 2 = 12
    CHECK(Near(child.Get<TransformComponent>().WorldPosition().x, 12));
}

static void TestParentRotationAffectsChild()
{
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    Entity child  = scene.CreateEntity("child", parent);
    parent.Get<TransformComponent>().SetEulerDegrees(0, 90, 0);          // yaw 90deg: +Z -> +X
    child.Get<TransformComponent>().position = { 0, 0, 5 };              // 5 units in front of the parent
    TransformSystem::Update(scene);
    auto w = child.Get<TransformComponent>().WorldPosition();
    CHECK(Near(w.x, 5) && Near(w.y, 0) && Near(w.z, 0));
    auto f = parent.Get<TransformComponent>().WorldForward();
    CHECK(Near(f.x, 1) && Near(f.z, 0));
}

static void TestDeepHierarchyCreatedOutOfOrder()
{
    Scene scene;
    // create the grandchild FIRST in storage order, then wire the chain: ordering must not depend on creation order
    Entity c = scene.CreateEntity("c"); Entity b = scene.CreateEntity("b"); Entity a = scene.CreateEntity("a");
    CHECK(scene.SetParent(c, b)); CHECK(scene.SetParent(b, a));
    a.Get<TransformComponent>().position = { 1, 0, 0 };
    b.Get<TransformComponent>().position = { 10, 0, 0 };
    c.Get<TransformComponent>().position = { 100, 0, 0 };
    TransformSystem::Update(scene);
    CHECK(Near(c.Get<TransformComponent>().WorldPosition().x, 111));
}

static void TestCyclesAreRejected()
{
    Scene scene;
    Entity a = scene.CreateEntity("a"), b = scene.CreateEntity("b"), c = scene.CreateEntity("c");
    CHECK(scene.SetParent(b, a)); CHECK(scene.SetParent(c, b));
    CHECK(!scene.SetParent(a, c));          // a under its own grandchild
    CHECK(!scene.SetParent(a, a));          // itself
    CHECK(!scene.GetParent(a));             // unchanged: still a root
    CHECK(scene.GetParent(c) == b);
    CHECK(scene.SetParent(c, Entity()));    // unparent
    CHECK(!scene.GetParent(c));
}

static void TestDestroyRemovesDescendantsAndStaleHandlesAreSafe()
{
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    Entity child = scene.CreateEntity("child", parent);
    Entity grand = scene.CreateEntity("grand", child);
    Entity other = scene.CreateEntity("other");
    CHECK(scene.EntityCount() == 4);

    scene.DestroyEntity(parent);
    CHECK(scene.EntityCount() == 1);
    CHECK(!parent && !child && !grand);                         // all stale
    CHECK(other.IsValid());
    CHECK(child.TryGet<TransformComponent>() == nullptr);       // no crash, just null
    CHECK(!child.Has<TransformComponent>());

    bool threw = false;
    try { child.Get<TransformComponent>(); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);                                               // a clear error instead of reading freed memory

    // a NEW entity may reuse the old slot; the old handle must NOT alias it
    Entity fresh = scene.CreateEntity("fresh");
    CHECK(fresh.IsValid());
    CHECK(!parent.IsValid());
}

static void TestComponentsAddReplaceRemove()
{
    Scene scene;
    Entity e = scene.CreateEntity("lamp");
    CHECK(!e.Has<LightComponent>());
    e.Add<LightComponent>(LightComponent{ LightType::Spot, { 1, 0, 0 }, 5.0f, 20.0f, 10.0f, 25.0f, true });
    CHECK(e.Has<LightComponent>() && e.Get<LightComponent>().type == LightType::Spot);
    e.Add<LightComponent>().intensity = 9.0f;                    // replaces with a default-constructed one
    CHECK(e.Get<LightComponent>().type == LightType::Point && Near(e.Get<LightComponent>().intensity, 9.0f));
    e.Remove<LightComponent>();
    CHECK(!e.Has<LightComponent>());
}

static void TestFindAndChildren()
{
    Scene scene;
    Entity root = scene.CreateEntity("root");
    Entity a = scene.CreateEntity("a", root), b = scene.CreateEntity("b", root);
    CHECK(scene.FindByName("b") == b);
    CHECK(!scene.FindByName("missing"));
    CHECK(scene.GetChildren(root).size() == 2);
    CHECK(scene.GetChildren(a).empty());
}

static void TestManyEntitiesUnderOneParent()
{
    Scene scene;
    Entity root = scene.CreateEntity("root");
    root.Get<TransformComponent>().position = { 0, 100, 0 };
    for (int i = 0; i < 5000; ++i)
        scene.CreateEntity("n", root).Get<TransformComponent>().position = { float(i), 0, 0 };
    TransformSystem::Update(scene);
    size_t ok = 0;
    for (auto [e, t] : scene.Registry().view<TransformComponent>().each())
        if (e != root.Id() && Near(t.WorldPosition().y, 100)) ++ok;
    CHECK(ok == 5000);
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "world from local",                         TestWorldFromLocal },
        { "parent/child composition",                 TestParentChildComposition },
        { "parent rotation affects child",            TestParentRotationAffectsChild },
        { "deep hierarchy, out-of-order creation",    TestDeepHierarchyCreatedOutOfOrder },
        { "cycles are rejected",                      TestCyclesAreRejected },
        { "destroy + stale handles are safe",         TestDestroyRemovesDescendantsAndStaleHandlesAreSafe },
        { "add / replace / remove component",         TestComponentsAddReplaceRemove },
        { "find by name, children",                   TestFindAndChildren },
        { "5000 children under one parent",           TestManyEntitiesUnderOneParent },
    };
    for (auto& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll scene tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
