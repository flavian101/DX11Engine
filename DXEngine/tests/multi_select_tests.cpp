// Multi-selection without a GPU: the selection set, the operations on the roots of a selection (delete, duplicate, reparent, move as
// a group), the bounds of a selection and the box select.
#include "camera/Camera.h"
#include "editor/EditorContext.h"
#include "editor/EditorSelection.h"
#include "editor/EntityBounds.h"
#include "editor/SceneEditing.h"
#include "editor/ViewportMath.h"
#include "scene/Scene.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include <cmath>
#include <cstdio>

using namespace DXEngine;
using namespace DirectX;

static int g_failed = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAILED line %d: %s\n", __LINE__, #c); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) <= e; }
static bool NearV(const XMFLOAT3& a, const XMFLOAT3& b, float e = 1e-3f) { return Near(a.x, b.x, e) && Near(a.y, b.y, e) && Near(a.z, b.z, e); }

static std::string NameOf(Entity e) { return e.Get<NameComponent>().name; }
static XMFLOAT3 WorldPos(Entity e) { return e.Get<TransformComponent>().WorldPosition(); }

// A mesh entity with a unit box (so it has bounds); no GPU needed.
static Entity MakeBox(Scene& scene, const char* name, XMFLOAT3 position, Entity parent = Entity())
{
    Entity e = parent.IsValid() ? scene.CreateEntity(name, parent) : scene.CreateEntity(name);
    e.Get<TransformComponent>().position = position;
    MeshRendererComponent r;
    r.mesh = std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("box", 1.0f)), nullptr);
    e.Add<MeshRendererComponent>(r);
    return e;
}

// ------------------------------------------------------------------------------------------------ the selection set
static void TestSelectionSet()
{
    Scene scene;
    Entity a = scene.CreateEntity("a"), b = scene.CreateEntity("b"), c = scene.CreateEntity("c");
    EditorSelection s;
    CHECK(!s.Has() && s.Count() == 0 && !s.Get().IsValid() && s.All().empty());

    s.Set(a);
    CHECK(s.Has() && s.Count() == 1 && s.Get() == a && s.Is(a) && s.IsPrimary(a) && !s.Is(b));

    s.Add(b); s.Add(c);                                            // the last one added is the primary
    CHECK(s.Count() == 3 && s.Get() == c && s.IsPrimary(c) && !s.IsPrimary(a) && s.Is(a) && s.Is(b));
    CHECK((s.All() == std::vector<Entity>{ a, b, c }));

    s.Add(a);                                                      // already in: it only becomes the primary
    CHECK(s.Count() == 3 && s.Get() == a && (s.All() == std::vector<Entity>{ b, c, a }));

    s.Toggle(b);                                                   // leaves
    CHECK(s.Count() == 2 && !s.Is(b) && s.Get() == a);
    s.Toggle(b);                                                   // joins again, as the primary
    CHECK(s.Count() == 3 && s.Get() == b);
    s.Remove(b);
    CHECK(s.Get() == a && !s.Is(b));                               // the primary is the one selected before it

    s.Set(c);
    CHECK(s.Count() == 1 && s.Get() == c && !s.Is(a));             // Set replaces

    s.SetMany({ a, b, a, Entity(), c });                           // in this order, once each, no invalid ones
    CHECK((s.All() == std::vector<Entity>{ b, a, c }) && s.Get() == c);
    s.SetMany({});
    CHECK(!s.Has());

    s.Set(Entity()); s.Add(Entity()); s.Toggle(Entity());
    CHECK(!s.Has() && s.Count() == 0);

    // destroyed entities drop out by themselves
    s.SetMany({ a, b, c });
    scene.DestroyEntity(c);
    CHECK(s.Count() == 2 && s.Get() == b && !s.Is(c));
    scene.DestroyEntity(b); scene.DestroyEntity(a);
    CHECK(!s.Has() && s.Count() == 0 && s.All().empty());
    s.Clear();
}

// ------------------------------------------------------------------------------------------------ roots, delete, duplicate, reparent
struct Family
{
    Family()
    {
        parent = MakeBox(scene, "parent", { 0, 0, 0 });
        child = MakeBox(scene, "child", { 1, 0, 0 }, parent);
        grandchild = MakeBox(scene, "grandchild", { 1, 0, 0 }, child);
        other = MakeBox(scene, "other", { 5, 0, 0 });
        loner = MakeBox(scene, "loner", { -5, 0, 0 });
        TransformSystem::Update(scene);
    }
    Scene scene;
    Entity parent, child, grandchild, other, loner;
};

static void TestRoots()
{
    Family f;
    auto names = [](const std::vector<Entity>& v) { std::string s; for (Entity e : v) s += NameOf(e) + ","; return s; };

    CHECK(names(SceneEditing::Roots(f.scene, { f.parent, f.child, f.other })) == "parent,other,");        // the child rides with its parent
    CHECK(names(SceneEditing::Roots(f.scene, { f.child, f.grandchild })) == "child,");                    // two levels: the upper one
    CHECK(names(SceneEditing::Roots(f.scene, { f.grandchild, f.parent })) == "parent,");                  // whatever the order
    CHECK(names(SceneEditing::Roots(f.scene, { f.child, f.other, f.child })) == "child,other,");          // once each
    CHECK(names(SceneEditing::Roots(f.scene, { Entity(), f.loner })) == "loner,");                        // invalid ones are left out

    Scene elsewhere; Entity foreign = elsewhere.CreateEntity("foreign");
    CHECK(names(SceneEditing::Roots(f.scene, { foreign, f.loner })) == "loner,");                         // another scene's too
    CHECK(SceneEditing::Roots(f.scene, {}).empty());
}

static void TestAllEntities()
{
    Family f;
    Entity sky = f.scene.CreateEntity("sky"); sky.Add<SkyboxComponent>();
    const auto all = SceneEditing::AllEntities(f.scene);
    CHECK(all.size() == 5 && std::find(all.begin(), all.end(), sky) == all.end());      // everything but the sky, children included
    CHECK(std::find(all.begin(), all.end(), f.grandchild) != all.end());
    Scene empty;
    CHECK(SceneEditing::AllEntities(empty).empty());
}

static void TestDestroyAndDuplicate()
{
    Family f;
    const size_t start = f.scene.EntityCount();                                                            // 5

    // delete a parent together with its child: the parent takes the child along, nothing is deleted twice
    CHECK(SceneEditing::DestroyEntities(f.scene, { f.parent, f.child, f.loner }) == 2);
    CHECK(f.scene.EntityCount() == start - 4 && !f.parent.IsValid() && !f.child.IsValid() && !f.grandchild.IsValid() && !f.loner.IsValid() && f.other.IsValid());
    CHECK(SceneEditing::DestroyEntities(f.scene, {}) == 0);

    Family g;
    const size_t n = g.scene.EntityCount();
    const auto copies = SceneEditing::DuplicateEntities(g.scene, { g.parent, g.grandchild, g.other });    // the grandchild is below the parent: one copy of the family
    CHECK(copies.size() == 2 && g.scene.EntityCount() == n + 3 + 1);                                       // parent + child + grandchild, and other
    CHECK(NameOf(copies[0]) == "parent (Copy)" && NameOf(copies[1]) == "other (Copy)");
    CHECK(g.scene.GetChildren(copies[0]).size() == 1);                                                      // the copy has its own subtree
    CHECK(g.scene.GetChildren(g.parent).size() == 1 && g.child.IsValid() && g.scene.GetParent(g.child) == g.parent);   // the originals are as they were
    CHECK(SceneEditing::CanDuplicateAny(g.scene, { g.loner }) && !SceneEditing::CanDuplicateAny(g.scene, {}));

    // the sky cannot be duplicated: it is skipped, the rest is copied
    Entity sky = g.scene.CreateEntity("sky"); sky.Add<SkyboxComponent>();
    const auto some = SceneEditing::DuplicateEntities(g.scene, { sky, g.loner });
    CHECK(some.size() == 1 && NameOf(some[0]) == "loner (Copy)");
    CHECK(!SceneEditing::CanDuplicateAny(g.scene, { sky }) && SceneEditing::DuplicateEntities(g.scene, { sky }).empty());
}

static void TestReparentMany()
{
    Family f;
    f.scene.FindByName("other").Get<TransformComponent>().scale = { 2, 2, 2 };
    TransformSystem::Update(f.scene);
    const XMFLOAT3 loner = WorldPos(f.loner), child = WorldPos(f.child);

    // two entities and a descendant of one of them go under 'other'; none of them moves in the world
    CHECK(SceneEditing::ReparentEntities(f.scene, { f.loner, f.parent, f.child }, f.other) == 2);
    TransformSystem::Update(f.scene);
    CHECK(f.scene.GetParent(f.loner) == f.other && f.scene.GetParent(f.parent) == f.other && f.scene.GetParent(f.child) == f.parent);
    CHECK(NearV(WorldPos(f.loner), loner) && NearV(WorldPos(f.child), child));

    // a root cannot go below itself or its own descendant: it stays where it is, the others go
    Family g;
    CHECK(SceneEditing::ReparentEntities(g.scene, { g.parent, g.loner }, g.grandchild) == 1);
    CHECK(!g.scene.GetParent(g.parent).IsValid() && g.scene.GetParent(g.loner) == g.grandchild);
    CHECK(SceneEditing::ReparentEntities(g.scene, { g.other }, g.other) == 0);                              // not below itself

    // unparent: to nothing
    CHECK(SceneEditing::ReparentEntities(g.scene, { g.loner }, Entity()) == 1 && !g.scene.GetParent(g.loner).IsValid());
}

// ------------------------------------------------------------------------------------------------ moving a group with the gizmo
static XMFLOAT4X4 WorldMatrix(Entity e) { XMFLOAT4X4 m; XMStoreFloat4x4(&m, e.Get<TransformComponent>().World()); return m; }

static bool SameMatrix(const XMFLOAT4X4& a, const XMFLOAT4X4& b, float e = 2e-3f)
{
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) if (!Near(a.m[r][c], b.m[r][c], e)) return false;
    return true;
}

static void TestGroupMoveMovesEveryoneRigidly()
{
    Scene scene;
    // a is the pivot, b hangs under a parent that is rotated and scaled (the hard case for "world matrix -> local transform")
    Entity a = MakeBox(scene, "a", { 1, 0, 0 });
    Entity p = MakeBox(scene, "p", { 4, 0, -3 });
    p.Get<TransformComponent>().scale = { 2, 2, 2 };
    p.Get<TransformComponent>().SetEulerDegrees(0, 50, 0);
    Entity b = MakeBox(scene, "b", { 0.5f, 0.7f, 0.0f }, p);
    Entity c = MakeBox(scene, "c", { -3, 2, 1 });
    TransformSystem::Update(scene);

    const XMFLOAT4X4 before = WorldMatrix(a);

    // 1. translate the pivot by (3, 0, 0) in the world: everyone moves by (3, 0, 0) and keeps its orientation and size
    {
        XMFLOAT4X4 after = before; after.m[3][0] += 3.0f;
        const XMFLOAT4X4 bw = WorldMatrix(b), cw = WorldMatrix(c);
        CHECK(SceneEditing::ApplyWorldDelta(scene, { a, b, c }, before, after));
        TransformSystem::Update(scene);
        XMFLOAT4X4 bExpected = bw, cExpected = cw;
        bExpected.m[3][0] += 3.0f; cExpected.m[3][0] += 3.0f;
        CHECK(SameMatrix(WorldMatrix(a), after));
        CHECK(SameMatrix(WorldMatrix(b), bExpected));
        CHECK(SameMatrix(WorldMatrix(c), cExpected));
    }

    // 2. rotate the group 90 degrees about the world Y axis through the pivot: the others orbit it
    {
        const XMFLOAT4X4 pivotBefore = WorldMatrix(a);
        const XMFLOAT3 pos = WorldPos(a);
        const XMMATRIX t = XMMatrixTranslation(-pos.x, -pos.y, -pos.z) * XMMatrixRotationY(XM_PIDIV2) * XMMatrixTranslation(pos.x, pos.y, pos.z);
        XMFLOAT4X4 after; XMStoreFloat4x4(&after, XMLoadFloat4x4(&pivotBefore) * t);
        const XMFLOAT4X4 bw = WorldMatrix(b), cw = WorldMatrix(c);
        CHECK(SceneEditing::ApplyWorldDelta(scene, { a, b, c }, pivotBefore, after));
        TransformSystem::Update(scene);
        XMFLOAT4X4 bExpected, cExpected;
        XMStoreFloat4x4(&bExpected, XMLoadFloat4x4(&bw) * t);
        XMStoreFloat4x4(&cExpected, XMLoadFloat4x4(&cw) * t);
        CHECK(SameMatrix(WorldMatrix(b), bExpected, 5e-3f));
        CHECK(SameMatrix(WorldMatrix(c), cExpected, 5e-3f));
    }

    // 3. the pivot is a DESCENDANT of a listed root: the root moves, the pivot (below it) ends exactly where the gizmo put it
    {
        const XMFLOAT4X4 pivotBefore = WorldMatrix(b);
        XMFLOAT4X4 after = pivotBefore; after.m[3][1] += 2.0f;
        const XMFLOAT4X4 cw = WorldMatrix(c);
        CHECK(SceneEditing::ApplyWorldDelta(scene, { p, b, c }, pivotBefore, after));      // roots: p and c
        TransformSystem::Update(scene);
        CHECK(SameMatrix(WorldMatrix(b), after, 5e-3f));
        XMFLOAT4X4 cExpected = cw; cExpected.m[3][1] += 2.0f;
        CHECK(SameMatrix(WorldMatrix(c), cExpected));
    }

    // a pivot that cannot be inverted (zero scale): nothing moves
    {
        Entity flat = MakeBox(scene, "flat", { 9, 9, 9 });
        flat.Get<TransformComponent>().scale = { 0, 1, 1 };
        TransformSystem::Update(scene);
        const XMFLOAT3 where = WorldPos(c);
        XMFLOAT4X4 after = WorldMatrix(flat); after.m[3][0] += 1.0f;
        CHECK(!SceneEditing::ApplyWorldDelta(scene, { flat, c }, WorldMatrix(flat), after));
        TransformSystem::Update(scene);
        CHECK(NearV(WorldPos(c), where));
    }

    // one entity: the same as putting it at the new matrix
    {
        Entity solo = MakeBox(scene, "solo", { 0, 0, 0 });
        TransformSystem::Update(scene);
        XMFLOAT4X4 after = WorldMatrix(solo); after.m[3][2] -= 4.0f;
        CHECK(SceneEditing::ApplyWorldDelta(scene, { solo }, WorldMatrix(solo), after));
        TransformSystem::Update(scene);
        CHECK(Near(WorldPos(solo).z, -4.0f));
    }
}

// ------------------------------------------------------------------------------------------------ bounds of a selection, box select
static void TestSelectionBounds()
{
    Scene scene;
    Entity a = MakeBox(scene, "a", { 0, 0, 0 }), b = MakeBox(scene, "b", { 10, 0, 0 });
    Entity marker = scene.CreateEntity("marker"); marker.Get<TransformComponent>().position = { 0, 20, 0 };
    TransformSystem::Update(scene);

    const EntityBounds one = ComputeEntityBounds(a);
    const EntityBounds two = ComputeSelectionBounds({ a, b });
    CHECK(!two.hasBox && NearV(two.center, { 5, 0, 0 }) && two.radius > 5.0f && two.radius < 6.5f);       // reaches the far corners of both boxes
    CHECK(Near(ComputeSelectionBounds({ a }).radius, one.radius) && NearV(ComputeSelectionBounds({ a }).center, one.center));
    const EntityBounds withMarker = ComputeSelectionBounds({ a, marker });                                 // an entity without a mesh counts by its position
    CHECK(NearV(withMarker.center, { 0, 10, 0 }, 0.6f) && withMarker.radius >= 10.0f);
    const EntityBounds none = ComputeSelectionBounds({ Entity() });
    CHECK(NearV(none.center, { 0, 0, 0 }));
}

static void TestBoxSelect()
{
    Scene scene;
    Entity left = MakeBox(scene, "left", { -3, 0, 0 }), middle = MakeBox(scene, "middle", { 0, 0, 0 }), right = MakeBox(scene, "right", { 3, 0, 0 });
    Entity distant = MakeBox(scene, "distant", { 0, 0, -50 });                                                    // far away, still straight ahead
    Entity sky = MakeBox(scene, "sky", { 0, 0, 0 }); sky.Add<SkyboxComponent>();
    Entity behind = MakeBox(scene, "behind", { 0, 0, 30 });                                               // behind the camera
    TransformSystem::Update(scene);

    const float W = 800.0f, H = 600.0f;
    Camera camera;
    camera.SetProjectionParams(XM_PIDIV4, W / H, 0.5f, 1000.0f);
    camera.SetPosition({ 0, 0, 10 });
    camera.Update(0.0f);
    const XMFLOAT4X4 view = camera.GetViewMatrix4X4(), proj = camera.GetProjection4X4();

    auto where = [&](Entity e) { XMFLOAT2 s; ViewportMath::ProjectToViewport(ComputeEntityBounds(e).center, view, proj, W, H, s); return s; };
    auto names = [](const std::vector<Entity>& v) { std::string s; for (Entity e : v) s += NameOf(e) + ","; return s; };
    auto has = [&](const std::vector<Entity>& v, Entity e) { return std::find(v.begin(), v.end(), e) != v.end(); };

    // everything: the three in front (the sky and what is behind the camera are never picked)
    const auto all = EntitiesInRect(scene, view, proj, W, H, 0, 0, W, H);
    CHECK(has(all, left) && has(all, middle) && has(all, right) && has(all, distant));
    CHECK(!has(all, sky) && !has(all, behind));

    // a box around the left one only; corners given in any order
    const XMFLOAT2 l = where(left);
    CHECK(names(EntitiesInRect(scene, view, proj, W, H, l.x - 20, l.y - 20, l.x + 20, l.y + 20)) == "left,");
    CHECK(names(EntitiesInRect(scene, view, proj, W, H, l.x + 20, l.y + 20, l.x - 20, l.y - 20)) == "left,");

    // left and middle but not right
    const XMFLOAT2 m = where(middle), r = where(right);
    CHECK((l.x < m.x && m.x < r.x) || (l.x > m.x && m.x > r.x));                                           // (which way +X points on screen is the camera's business)
    const auto two = EntitiesInRect(scene, view, proj, W, H, std::min(l.x, m.x) - 10, m.y - 10, std::max(l.x, m.x) + 10, m.y + 10);
    CHECK(has(two, left) && has(two, middle) && !has(two, right));

    // the distant entity is behind the middle one on screen: one box takes both
    CHECK(has(EntitiesInRect(scene, view, proj, W, H, m.x - 5, m.y - 5, m.x + 5, m.y + 5), distant));
    // an empty area: nothing
    CHECK(EntitiesInRect(scene, view, proj, W, H, 0, 0, 4, 4).empty());
}

int main()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "selection: an ordered set with a primary",       TestSelectionSet },
        { "roots of a list of entities",                    TestRoots },
        { "select all: everything but the sky",             TestAllEntities },
        { "delete and duplicate the roots",                 TestDestroyAndDuplicate },
        { "reparent many, keeps world positions",           TestReparentMany },
        { "group move: rigid, under a rotated parent",      TestGroupMoveMovesEveryoneRigidly },
        { "bounds of a selection",                          TestSelectionBounds },
        { "box select",                                     TestBoxSelect },
    };
    for (const Test& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll multi-selection tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
