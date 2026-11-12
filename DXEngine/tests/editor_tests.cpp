// Editor logic tests: selection, the hierarchy rows, scene operations (duplicate, reparent, gizmo write-back),
// transform editing and viewport maths. No GPU, no window, no ImGui: plain asserts so they run anywhere.
//   build: see DXEngine/tests/CMakeLists.txt  (cmake -DDXENGINE_BUILD_TESTS=ON)
#include "editor/EditorSelection.h"
#include "editor/GizmoTypes.h"
#include "editor/HierarchyModel.h"
#include "editor/SceneEditing.h"
#include "editor/TransformEditing.h"
#include "editor/ViewportMath.h"
#include "scene/TransformSystem.h"
#include <cmath>
#include <cstdio>

using namespace DXEngine;
using namespace DirectX;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAILED line %d: %s\n", __LINE__, #cond); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

static XMFLOAT3 WorldPos(Entity e) { return e.Get<TransformComponent>().WorldPosition(); }
static std::string NameOf(Entity e) { return e.Get<NameComponent>().name; }

// ------------------------------------------------------------ selection
static void TestSelection()
{
    Scene scene;
    EditorSelection sel;
    CHECK(!sel.Has());
    CHECK(!sel.Get().IsValid());

    Entity a = scene.CreateEntity("a");
    Entity b = scene.CreateEntity("b");
    sel.Set(a);
    CHECK(sel.Has() && sel.Is(a) && !sel.Is(b));
    CHECK(sel.Get() == a);

    // a destroyed entity cannot be read through the selection
    scene.DestroyEntity(a);
    CHECK(!sel.Has());
    CHECK(!sel.Get().IsValid());
    CHECK(!sel.Is(a));

    // ... and a recycled id is not mistaken for it
    Entity c = scene.CreateEntity("c");
    CHECK(!sel.Is(c));

    sel.Set(b);
    sel.Clear();
    CHECK(!sel.Has());
}

// ------------------------------------------------------------ hierarchy rows
static void TestHierarchyRows()
{
    Scene scene;
    Entity root1 = scene.CreateEntity("root1");
    Entity kidA  = scene.CreateEntity("kidA", root1);
    Entity kidB  = scene.CreateEntity("kidB", root1);
    Entity deep  = scene.CreateEntity("deep", kidA);
    Entity root2 = scene.CreateEntity("root2");

    HierarchyModel model;
    model.Rebuild(scene);
    // everything starts collapsed: only the roots, in creation order
    CHECK(model.Rows().size() == 2);
    CHECK(model.Rows()[0].entity == root1 && model.Rows()[0].hasChildren && !model.Rows()[0].expanded);
    CHECK(model.Rows()[1].entity == root2 && !model.Rows()[1].hasChildren);

    model.SetExpanded(root1, true);
    model.Rebuild(scene);
    CHECK(model.Rows().size() == 4);                         // root1, kidA, kidB, root2
    CHECK(model.Rows()[1].entity == kidA && model.Rows()[1].depth == 1 && model.Rows()[1].hasChildren);
    CHECK(model.Rows()[2].entity == kidB && model.Rows()[2].depth == 1 && !model.Rows()[2].hasChildren);
    CHECK(model.Rows()[3].entity == root2 && model.Rows()[3].depth == 0);

    model.SetExpanded(kidA, true);
    model.Rebuild(scene);
    CHECK(model.Rows().size() == 5);
    CHECK(model.Rows()[2].entity == deep && model.Rows()[2].depth == 2);   // right after its parent

    // collapsing a parent hides the whole subtree but remembers the child's own state
    model.Toggle(root1);
    model.Rebuild(scene);
    CHECK(model.Rows().size() == 2);
    model.Toggle(root1);
    model.Rebuild(scene);
    CHECK(model.Rows().size() == 5);
}

static void TestHierarchyStateSurvivesAndPrunes()
{
    Scene scene;
    Entity a = scene.CreateEntity("a");
    scene.CreateEntity("a child", a);
    Entity b = scene.CreateEntity("b");
    scene.CreateEntity("b child", b);

    HierarchyModel model;
    model.SetExpanded(a, true);
    model.SetExpanded(b, true);
    model.Rebuild(scene);
    CHECK(model.Rows().size() == 4);

    // destroying an entity does not disturb the others, and its expanded entry is dropped
    scene.DestroyEntity(a);
    model.Rebuild(scene);
    CHECK(model.Rows().size() == 2);
    CHECK(model.Rows()[0].entity == b && model.Rows()[0].expanded);
    CHECK(!model.IsExpanded(a));
}

static void TestHierarchyOrderIsStable()
{
    Scene scene;
    Entity a = scene.CreateEntity("a");
    Entity b = scene.CreateEntity("b");
    Entity c = scene.CreateEntity("c");
    Entity d = scene.CreateEntity("d");

    HierarchyModel model;
    model.Rebuild(scene);
    CHECK(model.Rows().size() == 4);
    CHECK(model.Rows()[0].entity == a && model.Rows()[1].entity == b && model.Rows()[2].entity == c && model.Rows()[3].entity == d);

    // the storage swaps the last entity into a destroyed one's slot; the rows must not shuffle because of it
    scene.DestroyEntity(b);
    model.Rebuild(scene);
    CHECK(model.Rows().size() == 3);
    CHECK(model.Rows()[0].entity == a && model.Rows()[1].entity == c && model.Rows()[2].entity == d);
}

static void TestHierarchyFilterAndReveal()
{
    Scene scene;
    Entity sun   = scene.CreateEntity("Sun Light");
    Entity group = scene.CreateEntity("Group");
    Entity lamp  = scene.CreateEntity("Street LAMP", group);
    scene.CreateEntity("Rock", group);

    HierarchyModel model;
    model.Rebuild(scene, "lamp");                            // case-insensitive, flat, finds nested entities
    CHECK(model.IsFiltering());
    CHECK(model.Rows().size() == 1 && model.Rows()[0].entity == lamp && model.Rows()[0].depth == 0);

    model.Rebuild(scene, "light");
    CHECK(model.Rows().size() == 1 && model.Rows()[0].entity == sun);

    model.Rebuild(scene, "zzz");
    CHECK(model.Rows().empty());

    model.Rebuild(scene, "");                                // back to the tree
    CHECK(!model.IsFiltering() && model.Rows().size() == 2);

    // revealing a nested entity expands the chain above it
    model.RevealEntity(scene, lamp);
    model.Rebuild(scene);
    bool found = false;
    for (const HierarchyRow& r : model.Rows()) if (r.entity == lamp) found = true;
    CHECK(found);

    CHECK(ContainsNoCase("Hello World", "o w"));
    CHECK(ContainsNoCase("abc", ""));
    CHECK(!ContainsNoCase("abc", "abcd"));
}

static void TestHierarchyManyRoots()
{
    Scene scene;
    for (int i = 0; i < 2000; ++i) scene.CreateEntity("light " + std::to_string(i));
    HierarchyModel model;
    model.Rebuild(scene);
    CHECK(model.Rows().size() == 2000);
    CHECK(NameOf(model.Rows()[1999].entity) == "light 1999");
    // rebuilding again reuses its buffers and gives the same rows
    model.Rebuild(scene);
    CHECK(model.Rows().size() == 2000);
}

// ------------------------------------------------------------ duplicate
static void TestDuplicateSubtree()
{
    Scene scene;
    Entity parent = scene.CreateEntity("Holder");
    Entity thing  = scene.CreateEntity("Thing", parent);
    Entity part   = scene.CreateEntity("Part", thing);

    thing.Get<TransformComponent>().position = { 1, 2, 3 };
    thing.Get<TransformComponent>().scale = { 2, 2, 2 };
    MeshRendererComponent mr; mr.visible = false; mr.castsShadows = false; mr.receivesShadows = false;
    thing.Add<MeshRendererComponent>(mr);
    LightComponent light; light.type = LightType::Spot; light.intensity = 7.0f; light.castsShadows = true;
    part.Add<LightComponent>(light);

    const size_t before = scene.EntityCount();
    CHECK(SceneEditing::CanDuplicate(scene, thing));
    Entity copy = SceneEditing::Duplicate(scene, thing);
    CHECK(copy.IsValid() && copy != thing);
    CHECK(scene.EntityCount() == before + 2);                // Thing and Part

    CHECK(NameOf(copy) == "Thing (Copy)");
    CHECK(scene.GetParent(copy) == parent);                  // next to the original
    const TransformComponent& ct = copy.Get<TransformComponent>();
    CHECK(Near(ct.position.x, 1) && Near(ct.position.y, 2) && Near(ct.position.z, 3) && Near(ct.scale.x, 2));
    CHECK(copy.Has<MeshRendererComponent>());
    CHECK(!copy.Get<MeshRendererComponent>().visible && !copy.Get<MeshRendererComponent>().castsShadows && !copy.Get<MeshRendererComponent>().receivesShadows);

    std::vector<Entity> kids = scene.GetChildren(copy);
    CHECK(kids.size() == 1);
    CHECK(kids[0] != part && NameOf(kids[0]) == "Part");     // the child keeps its name, root only gets "(Copy)"
    CHECK(kids[0].Has<LightComponent>());
    CHECK(kids[0].Get<LightComponent>().type == LightType::Spot && Near(kids[0].Get<LightComponent>().intensity, 7.0f) && kids[0].Get<LightComponent>().castsShadows);

    // the original is untouched, and editing the copy does not change it
    copy.Get<TransformComponent>().position = { 9, 9, 9 };
    CHECK(Near(thing.Get<TransformComponent>().position.x, 1));
    CHECK(scene.GetChildren(thing).size() == 1 && scene.GetChildren(thing)[0] == part);
}

static void TestDuplicateRefusesSkinnedAndSky()
{
    Scene scene;
    Entity skinned = scene.CreateEntity("skinned");
    skinned.Add<SkinnedMeshComponent>();
    Entity holder = scene.CreateEntity("holder");
    scene.CreateEntity("animated child", holder).Add<AnimationUpdateComponent>();
    Entity sky = scene.CreateEntity("sky");
    sky.Add<SkyboxComponent>();
    Entity plain = scene.CreateEntity("plain");

    const size_t before = scene.EntityCount();
    CHECK(!SceneEditing::CanDuplicate(scene, skinned));
    CHECK(!SceneEditing::CanDuplicate(scene, holder));       // a refused descendant refuses the whole subtree
    CHECK(!SceneEditing::CanDuplicate(scene, sky));
    CHECK(SceneEditing::CanDuplicate(scene, plain));
    CHECK(!SceneEditing::Duplicate(scene, skinned).IsValid());
    CHECK(!SceneEditing::Duplicate(scene, holder).IsValid());
    CHECK(scene.EntityCount() == before);                    // nothing half-copied
    CHECK(!SceneEditing::Duplicate(scene, Entity()).IsValid());
}

// ------------------------------------------------------------ reparent keeps the world position
static void TestReparentKeepsWorld()
{
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    parent.Get<TransformComponent>().position = { 10, 0, 0 };
    parent.Get<TransformComponent>().scale = { 2, 2, 2 };
    parent.Get<TransformComponent>().SetEulerDegrees(0, 90, 0);
    Entity child = scene.CreateEntity("child");
    child.Get<TransformComponent>().position = { 1, 5, 2 };
    child.Get<TransformComponent>().SetEulerDegrees(0, 30, 0);
    TransformSystem::Update(scene);
    const XMFLOAT3 before = WorldPos(child);

    CHECK(SceneEditing::Reparent(scene, child, parent));
    CHECK(scene.GetParent(child) == parent);
    TransformSystem::Update(scene);
    const XMFLOAT3 after = WorldPos(child);
    CHECK(Near(before.x, after.x) && Near(before.y, after.y) && Near(before.z, after.z));

    // the local transform changed to make that true (it is not the old local values)
    CHECK(!Near(child.Get<TransformComponent>().position.x, 1));

    // back to a root: still in the same place
    CHECK(SceneEditing::Reparent(scene, child, Entity()));
    TransformSystem::Update(scene);
    CHECK(!scene.GetParent(child).IsValid());
    CHECK(Near(before.x, WorldPos(child).x) && Near(before.y, WorldPos(child).y) && Near(before.z, WorldPos(child).z));
    CHECK(Near(child.Get<TransformComponent>().position.x, before.x));
}

static void TestReparentSubtreeFollows()
{
    Scene scene;
    Entity newParent = scene.CreateEntity("newParent");
    newParent.Get<TransformComponent>().position = { 0, 10, 0 };
    Entity mid = scene.CreateEntity("mid");
    mid.Get<TransformComponent>().position = { 3, 0, 0 };
    Entity leaf = scene.CreateEntity("leaf", mid);
    leaf.Get<TransformComponent>().position = { 0, 0, 4 };
    TransformSystem::Update(scene);
    const XMFLOAT3 leafBefore = WorldPos(leaf);

    CHECK(SceneEditing::Reparent(scene, mid, newParent));
    TransformSystem::Update(scene);
    CHECK(Near(WorldPos(mid).x, 3) && Near(WorldPos(mid).y, 0));
    CHECK(Near(WorldPos(leaf).x, leafBefore.x) && Near(WorldPos(leaf).y, leafBefore.y) && Near(WorldPos(leaf).z, leafBefore.z));
}

static void TestReparentRejectsCyclesAndForeignEntities()
{
    Scene scene;
    Entity a = scene.CreateEntity("a");
    Entity b = scene.CreateEntity("b", a);
    Entity c = scene.CreateEntity("c", b);
    a.Get<TransformComponent>().position = { 1, 2, 3 };
    TransformSystem::Update(scene);

    CHECK(!SceneEditing::Reparent(scene, a, c));            // under its own descendant
    CHECK(!SceneEditing::Reparent(scene, a, a));
    CHECK(!scene.GetParent(a).IsValid() && scene.GetParent(c) == b);
    CHECK(Near(a.Get<TransformComponent>().position.x, 1)); // nothing was changed

    Scene other;
    Entity foreign = other.CreateEntity("foreign");
    CHECK(!SceneEditing::Reparent(scene, a, foreign));
    CHECK(!SceneEditing::Reparent(scene, foreign, a));
    CHECK(!SceneEditing::Reparent(scene, Entity(), a));
}

// ------------------------------------------------------------ gizmo write-back
static void TestApplyWorldMatrix()
{
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    parent.Get<TransformComponent>().position = { 5, 0, 0 };
    parent.Get<TransformComponent>().scale = { 2, 2, 2 };
    parent.Get<TransformComponent>().SetEulerDegrees(0, 90, 0);
    Entity child = scene.CreateEntity("child", parent);
    TransformSystem::Update(scene);

    // the gizmo moves the child to world (7, 3, -4), unrotated, unit scale
    XMFLOAT4X4 target;
    XMStoreFloat4x4(&target, XMMatrixTranslation(7, 3, -4));
    CHECK(SceneEditing::ApplyWorldMatrix(scene, child, target));

    // the stored world matrix is right immediately (no one-frame lag) ...
    CHECK(Near(WorldPos(child).x, 7) && Near(WorldPos(child).y, 3) && Near(WorldPos(child).z, -4));
    // ... and after the transform system re-derives it from the new LOCAL values
    TransformSystem::Update(scene);
    CHECK(Near(WorldPos(child).x, 7) && Near(WorldPos(child).y, 3) && Near(WorldPos(child).z, -4));

    // a root entity: local = world
    Entity root = scene.CreateEntity("root");
    XMStoreFloat4x4(&target, XMMatrixScaling(3, 3, 3) * XMMatrixTranslation(1, 1, 1));
    CHECK(SceneEditing::ApplyWorldMatrix(scene, root, target));
    CHECK(Near(root.Get<TransformComponent>().position.x, 1) && Near(root.Get<TransformComponent>().scale.y, 3));

    // a parent that cannot be inverted: refuses instead of writing garbage
    Entity flat = scene.CreateEntity("flat");
    flat.Get<TransformComponent>().scale = { 0, 1, 1 };
    Entity under = scene.CreateEntity("under", flat);
    TransformSystem::Update(scene);
    const XMFLOAT3 before = under.Get<TransformComponent>().position;
    CHECK(!SceneEditing::ApplyWorldMatrix(scene, under, target));
    CHECK(Near(under.Get<TransformComponent>().position.x, before.x));

    CHECK(!SceneEditing::ApplyWorldMatrix(scene, Entity(), target));
}

// ------------------------------------------------------------ creating things
static void TestCreateLights()
{
    Scene scene;
    Entity holder = scene.CreateEntity("holder");

    Entity point = SceneEditing::CreateLight(scene, LightType::Point, holder);
    CHECK(point.Has<LightComponent>() && point.Get<LightComponent>().type == LightType::Point);
    CHECK(NameOf(point) == "Point Light" && scene.GetParent(point) == holder);

    Entity sun = SceneEditing::CreateLight(scene, LightType::Directional);
    CHECK(sun.Get<LightComponent>().type == LightType::Directional && !scene.GetParent(sun).IsValid());
    TransformSystem::Update(scene);
    CHECK(sun.Get<TransformComponent>().WorldForward().y < -0.5f);       // shines downward

    Entity spot = SceneEditing::CreateLight(scene, LightType::Spot);
    TransformSystem::Update(scene);
    CHECK(spot.Get<TransformComponent>().WorldForward().y < -0.99f);     // straight down

    Entity empty = SceneEditing::CreateEmpty(scene, "Empty");
    CHECK(empty.IsValid() && NameOf(empty) == "Empty" && !empty.Has<LightComponent>());
    CHECK(SceneEditing::CopyName("Cube") == "Cube (Copy)");
}

// ------------------------------------------------------------ euler editing
static void TestEulerRoundTrip()
{
    // every canonical angle triple (pitch within +-89 degrees) survives angles -> quaternion -> angles
    const float angles[] = { -170.f, -90.f, -45.f, -10.f, 0.f, 10.f, 45.f, 90.f, 170.f };
    const float pitches[] = { -89.f, -45.f, -10.f, 0.f, 10.f, 45.f, 89.f };
    int bad = 0;
    for (float p : pitches) for (float y : angles) for (float r : angles)
    {
        const XMFLOAT4 q = TransformEditing::EulerDegreesToQuaternion({ p, y, r });
        const XMFLOAT3 e = TransformEditing::QuaternionToEulerDegrees(q);
        // compare as ROTATIONS (angles near +-180 may wrap)
        const XMFLOAT4 q2 = TransformEditing::EulerDegreesToQuaternion(e);
        if (!TransformEditing::SameRotation(q, q2)) ++bad;
        if (std::fabs(e.x) > 90.01f) ++bad;
    }
    CHECK(bad == 0);

    // and the plain cases are what a person expects
    XMFLOAT3 e = TransformEditing::QuaternionToEulerDegrees(TransformEditing::EulerDegreesToQuaternion({ 20, 40, 60 }));
    CHECK(Near(e.x, 20, 0.01f) && Near(e.y, 40, 0.01f) && Near(e.z, 60, 0.01f));

    // matches TransformComponent::SetEulerDegrees
    TransformComponent t; t.SetEulerDegrees(15, -25, 35);
    CHECK(TransformEditing::SameRotation(t.rotation, TransformEditing::EulerDegreesToQuaternion({ 15, -25, 35 })));

    // straight up / down (gimbal): still the same rotation
    for (float pitch : { 90.f, -90.f })
    {
        const XMFLOAT4 q = TransformEditing::EulerDegreesToQuaternion({ pitch, 30, 10 });
        const XMFLOAT3 g = TransformEditing::QuaternionToEulerDegrees(q);
        CHECK(TransformEditing::SameRotation(q, TransformEditing::EulerDegreesToQuaternion(g)));
    }

    // q and -q are one rotation
    XMFLOAT4 q = TransformEditing::EulerDegreesToQuaternion({ 10, 20, 30 });
    XMFLOAT4 neg = { -q.x, -q.y, -q.z, -q.w };
    CHECK(TransformEditing::SameRotation(q, neg));
    CHECK(!TransformEditing::SameRotation(q, TransformEditing::EulerDegreesToQuaternion({ 10, 20, 31 })));
}

static void TestEulerCacheKeepsWhatTheUserTyped()
{
    TransformEditing::EulerCache cache;
    TransformComponent t;

    // first sight: derived from the rotation
    t.SetEulerDegrees(10, 20, 30);
    XMFLOAT3 shown = cache.Get(t.rotation);
    CHECK(Near(shown.x, 10, 0.01f) && Near(shown.y, 20, 0.01f) && Near(shown.z, 30, 0.01f));

    // the user types pitch 170: stored as a quaternion. Converting back would show (10, 180, 180); the cache keeps 170.
    t.rotation = cache.Set({ 170, 0, 0 });
    shown = cache.Get(t.rotation);
    CHECK(Near(shown.x, 170, 0.01f) && Near(shown.y, 0, 0.01f) && Near(shown.z, 0, 0.01f));
    CHECK(TransformEditing::QuaternionToEulerDegrees(t.rotation).x < 90.5f);      // (the plain conversion would have changed it)

    // something else rotates the entity (an animation, the gizmo): the display follows
    t.SetEulerDegrees(0, 45, 0);
    shown = cache.Get(t.rotation);
    CHECK(Near(shown.x, 0, 0.01f) && Near(shown.y, 45, 0.01f) && Near(shown.z, 0, 0.01f));

    // a new selection starts from scratch
    cache.Reset();
    t.SetEulerDegrees(5, 6, 7);
    shown = cache.Get(t.rotation);
    CHECK(Near(shown.x, 5, 0.01f) && Near(shown.y, 6, 0.01f) && Near(shown.z, 7, 0.01f));
}

// ------------------------------------------------------------ gizmo foundations
static void TestGizmoSettingsAndNullGizmo()
{
    GizmoSettings s;
    CHECK(s.operation == GizmoOperation::Translate && !s.snap);
    CHECK(s.SnapStep() == 0.0f);                       // snapping off: no step whatever the tool

    s.snap = true;
    CHECK(Near(s.SnapStep(), 0.5f));
    s.operation = GizmoOperation::Rotate;
    CHECK(Near(s.SnapStep(), 15.0f));
    s.operation = GizmoOperation::Scale;
    CHECK(Near(s.SnapStep(), 0.1f));
    s.operation = GizmoOperation::None;
    CHECK(s.SnapStep() == 0.0f);

    CHECK(Near(SnapToStep(1.3f, 0.5f), 1.5f) && Near(SnapToStep(1.2f, 0.5f), 1.0f) && Near(SnapToStep(-0.7f, 0.5f), -0.5f));
    CHECK(Near(SnapToStep(1.234f, 0.0f), 1.234f));     // step 0 = unchanged

    // the placeholder does nothing and says so
    NullGizmo gizmo;
    IGizmo& g = gizmo;
    XMFLOAT4X4 world; XMStoreFloat4x4(&world, XMMatrixTranslation(1, 2, 3));
    GizmoView view{};
    CHECK(!g.IsAvailable() && !g.IsUsing() && !g.IsOver());
    CHECK(!g.Manipulate(view, s, world));
    CHECK(Near(world._41, 1) && Near(world._42, 2) && Near(world._43, 3));
}

// ------------------------------------------------------------ viewport maths
static void TestChooseTargetSize()
{
    using namespace ViewportMath;
    Size s = ChooseTargetSize(800.7f, 450.2f, {});
    CHECK(s.width == 800 && s.height == 450);                        // whole pixels, fractions cut

    // a collapsed / not yet laid out panel keeps the previous size instead of asking for nothing
    s = ChooseTargetSize(0.0f, 300.0f, { 640, 360 });
    CHECK(s.width == 640 && s.height == 360);
    s = ChooseTargetSize(0.5f, 0.5f, { 640, 360 });
    CHECK(s.width == 640 && s.height == 360);
    s = ChooseTargetSize(0.0f, 0.0f, {});                            // ... and with no history the minimum
    CHECK(s.width == 1 && s.height == 1);
    s = ChooseTargetSize(-10.0f, 20.0f, {});
    CHECK(s.width == 1 && s.height == 1);

    CHECK(Near(AspectRatio(1600, 900), 16.0f / 9.0f));
    CHECK(Near(AspectRatio(0, 0), 1.0f));                            // never 0 / NaN
}

static void TestScreenToTarget()
{
    using namespace ViewportMath;
    float x = -1, y = -1;
    // image drawn at (100, 50), 400 x 200 on screen; texture 800 x 400 (2x): positions are scaled
    CHECK(ScreenToTarget(300, 150, 100, 50, 400, 200, 800, 400, x, y));
    CHECK(Near(x, 400) && Near(y, 200));
    CHECK(ScreenToTarget(100, 50, 100, 50, 400, 200, 800, 400, x, y));      // top-left pixel
    CHECK(Near(x, 0) && Near(y, 0));
    CHECK(!ScreenToTarget(99.9f, 100, 100, 50, 400, 200, 800, 400, x, y));  // outside on the left
    CHECK(!ScreenToTarget(500, 100, 100, 50, 400, 200, 800, 400, x, y));    // the right edge itself is outside
    CHECK(!ScreenToTarget(300, 250, 100, 50, 400, 200, 800, 400, x, y));
    CHECK(!ScreenToTarget(300, 150, 100, 50, 0, 200, 800, 400, x, y));      // an image with no size
    CHECK(!ScreenToTarget(300, 150, 100, 50, 400, 200, 0, 400, x, y));      // a target with no size
    // same size on screen and in the texture: no scaling
    CHECK(ScreenToTarget(110, 60, 100, 50, 400, 200, 400, 200, x, y) && Near(x, 10) && Near(y, 10));
}

static void TestProjectToViewport()
{
    using namespace ViewportMath;
    XMFLOAT4X4 view, proj;
    XMStoreFloat4x4(&view, XMMatrixLookAtLH(XMVectorSet(0, 0, -10, 1), XMVectorSet(0, 0, 0, 1), XMVectorSet(0, 1, 0, 0)));
    XMStoreFloat4x4(&proj, XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 2.0f, 0.1f, 100.0f));

    XMFLOAT2 p;
    CHECK(ProjectToViewport({ 0, 0, 0 }, view, proj, 800, 400, p));
    CHECK(Near(p.x, 400, 0.1f) && Near(p.y, 200, 0.1f));              // on the view axis: the centre

    CHECK(ProjectToViewport({ 2, 0, 0 }, view, proj, 800, 400, p));
    CHECK(p.x > 400 && Near(p.y, 200, 0.1f));                         // +x is to the right
    CHECK(ProjectToViewport({ 0, 2, 0 }, view, proj, 800, 400, p));
    CHECK(Near(p.x, 400, 0.1f) && p.y < 200);                         // +y is UP, and screen y grows downward

    // the edge of the view: tan(30 deg) * 10 = 5.7735 high at distance 10
    CHECK(ProjectToViewport({ 0, 5.7735f, 0 }, view, proj, 800, 400, p));
    CHECK(Near(p.y, 0, 0.5f));

    CHECK(!ProjectToViewport({ 0, 0, -20 }, view, proj, 800, 400, p));   // behind the camera
    CHECK(!ProjectToViewport({ 0, 0, -10 }, view, proj, 800, 400, p));   // at the camera
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "selection: stale entity is nothing",             TestSelection },
        { "hierarchy: rows follow the expanded state",      TestHierarchyRows },
        { "hierarchy: state survives, dead entries pruned", TestHierarchyStateSurvivesAndPrunes },
        { "hierarchy: order is stable",                     TestHierarchyOrderIsStable },
        { "hierarchy: filter and reveal",                   TestHierarchyFilterAndReveal },
        { "hierarchy: 2000 roots",                          TestHierarchyManyRoots },
        { "duplicate: subtree, components, parent",         TestDuplicateSubtree },
        { "duplicate: refuses skinned / sky, copies nothing", TestDuplicateRefusesSkinnedAndSky },
        { "reparent: world position is kept",               TestReparentKeepsWorld },
        { "reparent: the subtree follows",                  TestReparentSubtreeFollows },
        { "reparent: cycles and foreign entities refused",  TestReparentRejectsCyclesAndForeignEntities },
        { "gizmo write-back: world matrix -> local TRS",    TestApplyWorldMatrix },
        { "create: lights and empties",                     TestCreateLights },
        { "euler: round trip, conventions, gimbal",         TestEulerRoundTrip },
        { "euler: cache keeps what the user typed",         TestEulerCacheKeepsWhatTheUserTyped },
        { "gizmo: settings, snapping, null gizmo",          TestGizmoSettingsAndNullGizmo },
        { "viewport: texture size from the panel",          TestChooseTargetSize },
        { "viewport: mouse -> texture pixel",               TestScreenToTarget },
        { "viewport: world point -> pixels",                TestProjectToViewport },
    };
    for (auto& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll editor tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
