// Play mode without a GPU: SceneSnapshot (take, restore, refuse, pins), GameRuntime (order, failures) and PlayMode (Start / Stop
// on an editor context: the scene, the selection, the camera, the editor's own entities, a game that fails).
#include "editor/EditorContext.h"
#include "editor/HierarchyModel.h"
#include "editor/PlayMode.h"
#include "models/ModelAsset.h"
#include "scene/GameRuntime.h"
#include "scene/Scene.h"
#include "scene/SceneSnapshot.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"
#include <cmath>
#include <cstdio>
#include <map>
#include <stdexcept>

using namespace DXEngine;

static int g_failed = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAILED line %d: %s\n", __LINE__, #c); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-5f) { return std::fabs(a - b) <= e; }

// ------------------------------------------------------------------------------------------------ fixtures
// A bridge with a few named meshes (no GPU, no files). A mesh that is not in the map has no name a file could use.
struct FakeBridge : IAssetBridge
{
    std::map<std::string, std::shared_ptr<Mesh>> meshes;
    int reuseCalls = 0;

    std::shared_ptr<Mesh> AddMesh(const std::string& name)
    {
        auto m = std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("cube", 1.0f)), nullptr);
        meshes[name] = m;
        return m;
    }
    std::string NameMesh(const Mesh& mesh) override { for (auto& [n, m] : meshes) if (m.get() == &mesh) return n; return {}; }
    std::string NameMaterial(const Material&) override { return {}; }                  // every material is the scene's own: saved by value
    std::string NameTexture(const RHI::ITexture&, TextureSlot) override { return {}; }
    std::string NameModelOfSkeleton(const Skeleton&) override { return {}; }
    std::shared_ptr<Mesh> ResolveMesh(const std::string& name) override { auto it = meshes.find(name); return it == meshes.end() ? nullptr : it->second; }
    std::shared_ptr<Material> ResolveMaterial(const std::string&) override { return nullptr; }
    std::shared_ptr<RHI::ITexture> ResolveTexture(const std::string&, TextureSlot) override { return nullptr; }
    std::shared_ptr<ModelAsset> ResolveModel(const std::string&) override { return nullptr; }
    void Reuse(Scene&) override { ++reuseCalls; }
};

// A small scene with everything play can change: a hierarchy, a mesh with a material of its own, a light.
struct World
{
    World()
    {
        mesh = bridge.AddMesh("primitive/cube?size=2");
        rig = scene.CreateEntity("Rig");
        rig.Get<TransformComponent>().position = { 1.0f, 2.0f, 3.0f };
        arm = scene.CreateEntity("Arm", rig);
        arm.Get<TransformComponent>().position = { 0.0f, 1.0f, 0.0f };
        paint = std::make_shared<Material>("paint", ShadingModel::Lit);
        paint->SetRoughness(0.3f);
        { MeshRendererComponent r; r.mesh = mesh; r.materials = { paint }; arm.Add<MeshRendererComponent>(r); }
        lamp = scene.CreateEntity("Lamp");
        { LightComponent l; l.type = LightType::Point; l.intensity = 300.0f; l.range = 40.0f; lamp.Add<LightComponent>(l); }
        TransformSystem::Update(scene);
    }

    std::string Text() { return SceneSerializer::Save(scene, bridge).text; }

    FakeBridge bridge;
    Scene scene;
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> paint;
    Entity rig, arm, lamp;
};

// What a careless game does to a scene: moves, edits values, deletes, creates.
static void Wreck(World& w)
{
    w.rig.Get<TransformComponent>().position = { -50.0f, 0.0f, 0.0f };
    w.lamp.Get<LightComponent>().intensity = 1.0f;
    w.arm.Get<MeshRendererComponent>().materials[0]->SetRoughness(0.9f);
    w.arm.Get<MeshRendererComponent>().visible = false;
    w.scene.DestroyEntity(w.lamp);
    Entity extra = w.scene.CreateEntity("Spawned", w.rig);
    extra.Add<LightComponent>();
    TransformSystem::Update(w.scene);
}

static bool SameText(const std::string& a, const std::string& b)
{
    if (a == b) return true;
    size_t i = 0, line = 1;
    while (i < a.size() && i < b.size() && a[i] == b[i]) { if (a[i] == '\n') ++line; ++i; }
    std::printf("    texts differ near line %zu (offset %zu)\n", line, i);
    return false;
}

// ------------------------------------------------------------------------------------------------ SceneSnapshot
static void TestSnapshotRestoresEverything()
{
    World w;
    const std::string before = w.Text();
    const Uuid rigId = w.scene.GetUuid(w.rig), lampId = w.scene.GetUuid(w.lamp);

    SceneSnapshot snap;
    CHECK(snap.IsEmpty());
    const auto taken = snap.Take(w.scene, w.bridge);
    CHECK(taken.ok && taken.warnings.empty() && !snap.IsEmpty());

    Wreck(w);
    CHECK(w.Text() != before);                                         // (the scene really changed)

    const auto back = snap.Restore(w.scene, w.bridge);
    CHECK(back.ok && back.warnings.empty() && back.entities == 3);
    CHECK(w.bridge.reuseCalls == 1);                                   // the bridge was told the old scene is still alive
    CHECK(SameText(before, w.Text()));                                  // every component, every value, the same order
    CHECK(w.scene.FindByUuid(rigId).IsValid() && w.scene.FindByUuid(lampId).IsValid());     // ids survive (handles of the old contents must not be used: one may even point at another entity now)

    // world matrices are current without anyone asking
    CHECK(TransformSystem::IsCurrent(w.scene));
    Entity arm = w.scene.FindByName("Arm");
    const auto p = arm.Get<TransformComponent>().WorldPosition();
    CHECK(Near(p.x, 1.0f) && Near(p.y, 3.0f) && Near(p.z, 3.0f));

    // the material of the scene is the scene's own VALUES again (a new object: the wrecked one is gone)
    CHECK(Near(arm.Get<MeshRendererComponent>().materials[0]->GetProperties().roughness, 0.3f));
    CHECK(arm.Get<MeshRendererComponent>().visible);

    // the snapshot stays: restoring a second time gives the same scene again
    w.scene.FindByName("Rig").Get<TransformComponent>().position = { 9, 9, 9 };
    CHECK(snap.Restore(w.scene, w.bridge).ok && SameText(before, w.Text()));
}

static void TestSnapshotRefusesWhatItCannotWrite()
{
    World w;
    SceneSnapshot snap;
    CHECK(snap.Take(w.scene, w.bridge).ok);
    const std::string kept = snap.Text();

    // a mesh made by code, with no name a file could use: restoring would drop it, so the snapshot is refused
    Entity stray = w.scene.CreateEntity("Stray");
    { MeshRendererComponent r; r.mesh = std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("x", 1.0f)), nullptr); stray.Add<MeshRendererComponent>(r); }
    const auto refused = snap.Take(w.scene, w.bridge);
    CHECK(!refused.ok && !refused.error.empty() && refused.warnings.size() == 1);
    CHECK(refused.warnings[0].find("Stray") != std::string::npos);      // it says WHICH entity
    CHECK(snap.Text() == kept);                                         // the earlier snapshot is untouched

    SceneSnapshot empty;
    CHECK(!empty.Take(w.scene, w.bridge).ok && empty.IsEmpty());
}

static void TestSnapshotRestoreProblems()
{
    World w;
    SceneSnapshot none;
    const std::string text = w.Text();
    const auto r = none.Restore(w.scene, w.bridge);
    CHECK(!r.ok && !r.error.empty());
    CHECK(w.Text() == text);                                            // nothing to restore: the scene is as it was

    // a mesh that cannot be found again: the entity comes back without it, and the warning says so
    SceneSnapshot snap;
    CHECK(snap.Take(w.scene, w.bridge).ok);
    w.bridge.meshes.clear();
    const auto back = snap.Restore(w.scene, w.bridge);
    CHECK(back.ok && !back.warnings.empty());
    CHECK(!w.scene.FindByName("Arm").Has<MeshRendererComponent>() && w.scene.FindByName("Lamp").Has<LightComponent>());
}

static void TestSnapshotPins()
{
    World w;
    SceneSnapshot snap;
    const size_t armPin = snap.Pin(w.scene, w.arm);
    const size_t nonePin = snap.Pin(w.scene, Entity());
    const Entity later = w.scene.CreateEntity("Later");
    const size_t laterPin = snap.Pin(w.scene, later);                    // exists when the snapshot is taken: it comes back
    CHECK(snap.PinCount() == 3);
    CHECK(snap.Take(w.scene, w.bridge).ok);

    const Entity madeAfter = w.scene.CreateEntity("MadeAfter");
    const size_t afterPin = snap.Pin(w.scene, madeAfter);               // not in the snapshot: gone after a restore

    CHECK(snap.Restore(w.scene, w.bridge).ok);
    Entity arm = snap.Pinned(w.scene, armPin);
    CHECK(arm.IsValid() && arm.Get<NameComponent>().name == "Arm");
    CHECK(!snap.Pinned(w.scene, nonePin).IsValid());
    CHECK(snap.Pinned(w.scene, laterPin).IsValid() && snap.Pinned(w.scene, laterPin).Get<NameComponent>().name == "Later");
    CHECK(!snap.Pinned(w.scene, afterPin).IsValid());
    CHECK(!snap.Pinned(w.scene, 99).IsValid());

    snap.Clear();
    CHECK(snap.IsEmpty() && snap.PinCount() == 0);
}

// ------------------------------------------------------------------------------------------------ GameRuntime
struct Recorder : IGameSystem
{
    Recorder(std::string n, std::vector<std::string>& l) : name(std::move(n)), log(l) {}
    void OnStart(Scene&) override { log.push_back(name + ":start"); if (throwOn == 1) throw std::runtime_error(name + " broke in start"); }
    void OnUpdate(Scene&, float dt) override { log.push_back(name + ":update"); updates += dt; if (throwOn == 2) throw std::runtime_error(name + " broke in update"); }
    void OnStop(Scene&) override { log.push_back(name + ":stop"); if (throwOn == 3) throw std::runtime_error(name + " broke in stop"); }
    std::string name; std::vector<std::string>& log; int throwOn = 0; float updates = 0.0f;
};

static Recorder* AddRecorder(GameRuntime& g, const char* name, std::vector<std::string>& log, int throwOn = 0)
{
    auto r = std::make_unique<Recorder>(name, log);
    r->throwOn = throwOn;
    Recorder* raw = r.get();
    g.Add(std::move(r));
    return raw;
}

static void TestGameRuntimeOrder()
{
    Scene scene;
    GameRuntime g;
    std::vector<std::string> log;
    CHECK(!g.Add(nullptr));
    AddRecorder(g, "a", log); Recorder* b = AddRecorder(g, "b", log);
    CHECK(g.SystemCount() == 2 && !g.IsRunning());

    CHECK(g.Update(scene, 0.1f) && log.empty());                         // not running: nothing happens
    CHECK(g.Start(scene) && g.IsRunning());
    CHECK(g.Start(scene));                                               // already running: no second start
    CHECK(!g.Add(std::make_unique<Recorder>("late", log)) && g.SystemCount() == 2);
    CHECK(g.Update(scene, 0.25f) && Near(b->updates, 0.25f));
    g.Stop(scene);
    g.Stop(scene);                                                       // already stopped: no second stop
    const std::vector<std::string> want = { "a:start", "b:start", "a:update", "b:update", "b:stop", "a:stop" };   // stop runs backwards
    CHECK(log == want);
    CHECK(!g.IsRunning() && g.LastError().empty());

    log.clear();
    CHECK(g.Start(scene) && g.Update(scene, 0.0f));                      // a stopped game can start again
    CHECK(log.size() == 4);
    g.Stop(scene);
}

static void TestGameRuntimeFailures()
{
    Scene scene;
    {   // a system that throws while running: the game stops itself, everyone gets OnStop, the error is kept
        GameRuntime g; std::vector<std::string> log;
        AddRecorder(g, "a", log); AddRecorder(g, "b", log, 2); AddRecorder(g, "c", log);
        CHECK(g.Start(scene));
        log.clear();
        CHECK(!g.Update(scene, 0.1f));
        CHECK(!g.IsRunning() && g.LastError() == "b broke in update");
        const std::vector<std::string> want = { "a:update", "b:update", "c:stop", "b:stop", "a:stop" };      // c never ran this frame
        CHECK(log == want);
        CHECK(g.Update(scene, 0.1f) && log.size() == want.size());       // stopped: later frames do nothing
    }
    {   // a system that throws in OnStart: those started before it are stopped, it is not, and the game is not running
        GameRuntime g; std::vector<std::string> log;
        AddRecorder(g, "a", log); AddRecorder(g, "b", log, 1); AddRecorder(g, "c", log);
        CHECK(!g.Start(scene) && !g.IsRunning() && g.LastError() == "b broke in start");
        const std::vector<std::string> want = { "a:start", "b:start", "a:stop" };
        CHECK(log == want);
    }
    {   // a system that throws in OnStop does not keep the others from stopping
        GameRuntime g; std::vector<std::string> log;
        AddRecorder(g, "a", log); AddRecorder(g, "b", log, 3);
        CHECK(g.Start(scene));
        log.clear();
        g.Stop(scene);
        const std::vector<std::string> want = { "b:stop", "a:stop" };
        CHECK(log == want && g.LastError() == "b broke in stop");
    }
}

// ------------------------------------------------------------------------------------------------ PlayMode
// A game that does something to the scene every frame.
struct Wrecker : IGameSystem
{
    explicit Wrecker(World& w) : world(w) {}
    void OnStart(Scene&) override { ++starts; }
    void OnUpdate(Scene&, float) override { if (!wrecked) { Wreck(world); wrecked = true; } }
    void OnStop(Scene&) override { ++stops; }
    World& world; bool wrecked = false; int starts = 0, stops = 0;
};

static std::unique_ptr<IAssetBridge> BridgeOf(World& w)
{
    // PlayMode takes a bridge per call: both calls name and find the same fake meshes
    struct Shared : FakeBridge { };
    auto b = std::make_unique<Shared>();
    b->meshes = w.bridge.meshes;
    return b;
}

static void TestPlayThenStopRestoresTheEditor()
{
    World w;
    EditorContext ctx;
    ctx.scene = &w.scene;
    ctx.camera = std::make_shared<CameraController>();
    Camera& cam = *ctx.camera->GetCamera();
    cam.SetPosition({ 1.0f, 2.0f, 3.0f }); cam.SetRotation({ -0.5f, 0.25f, 0.0f });
    ctx.selection.Set(w.arm);
    HierarchyModel tree;
    tree.SetExpanded(w.rig, true);
    const Entity stressParent = w.lamp;

    const std::string before = w.Text();
    const Uuid armId = w.scene.GetUuid(w.arm), rigId = w.scene.GetUuid(w.rig);

    GameRuntime game;
    auto wrecker = std::make_unique<Wrecker>(w);
    Wrecker* wr = wrecker.get();
    game.Add(std::move(wrecker));

    PlayMode play;
    std::vector<Entity> keep = { stressParent };
    for (Entity e : tree.ExpandedEntities(w.scene)) keep.push_back(e);
    CHECK(keep.size() == 2);

    const auto started = play.Start(ctx, &game, BridgeOf(w), keep);
    CHECK(started.ok && play.IsPlaying() && game.IsRunning() && wr->starts == 1);
    CHECK(play.Start(ctx, &game, BridgeOf(w)).ok == false);              // already playing

    // play: the game wrecks the scene, the fly camera goes somewhere else, the selection is deleted by the game
    CHECK(game.Update(w.scene, 0.016f));
    cam.SetPosition({ 100.0f, 100.0f, 100.0f }); cam.SetRotation({ 1.0f, 1.0f, 0.0f });
    CHECK(w.Text() != before);

    auto stopped = play.Stop(ctx, &game, BridgeOf(w));
    CHECK(stopped.ok && stopped.warnings.empty() && !play.IsPlaying() && !game.IsRunning() && wr->stops == 1);
    CHECK(SameText(before, w.Text()));                                    // the exit check: Play then Stop restores every component

    // the selection comes back by id, as a handle of the new scene
    CHECK(ctx.selection.Has() && ctx.selection.Get() == w.scene.FindByUuid(armId) && ctx.selection.Get().Get<NameComponent>().name == "Arm");

    // the editor's own entities come back in the order they were handed over
    CHECK(stopped.kept.size() == 2);
    CHECK(stopped.kept[0].IsValid() && stopped.kept[0].Get<NameComponent>().name == "Lamp");
    CHECK(stopped.kept[1].IsValid() && stopped.kept[1] == w.scene.FindByUuid(rigId));
    tree.SetExpanded(stopped.kept[1], true);
    tree.Rebuild(w.scene);
    bool armRow = false;
    for (HierarchyRow row : tree.Rows()) if (row.entity.Get<NameComponent>().name == "Arm") armRow = true;
    CHECK(armRow);                                                        // the expanded row is expanded again: its child shows

    // the editor camera is where it was
    const auto pos = cam.GetPositionFloat3(); const auto rot = cam.GetRotationFloat3();
    CHECK(Near(pos.x, 1.0f) && Near(pos.y, 2.0f) && Near(pos.z, 3.0f) && Near(rot.x, -0.5f) && Near(rot.y, 0.25f));

    CHECK(!play.Stop(ctx, &game, BridgeOf(w)).ok);                        // not playing: refused, nothing happens
    CHECK(SameText(before, w.Text()));
}

static void TestPlayIsRefusedWhenTheSceneCannotComeBack()
{
    World w;
    EditorContext ctx; ctx.scene = &w.scene;
    Entity stray = w.scene.CreateEntity("Stray");
    { MeshRendererComponent r; r.mesh = std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("x", 1.0f)), nullptr); stray.Add<MeshRendererComponent>(r); }

    GameRuntime game; std::vector<std::string> log;
    AddRecorder(game, "g", log);
    PlayMode play;
    const std::string before = w.Text();
    const auto r = play.Start(ctx, &game, BridgeOf(w));
    CHECK(!r.ok && !play.IsPlaying() && !game.IsRunning() && log.empty());   // the game never started
    CHECK(r.warnings.size() == 1 && r.warnings[0].find("Stray") != std::string::npos);
    CHECK(w.Text() == before);

    EditorContext none;
    CHECK(!play.Start(none, nullptr, BridgeOf(w)).ok);                    // no scene
    CHECK(!play.Start(ctx, nullptr, nullptr).ok);                         // no bridge
}

static void TestPlayWithoutGame()
{
    World w;
    EditorContext ctx; ctx.scene = &w.scene;
    PlayMode play;
    const std::string before = w.Text();
    CHECK(play.Start(ctx, nullptr, BridgeOf(w)).ok && play.IsPlaying());
    Wreck(w);
    CHECK(play.Stop(ctx, nullptr, BridgeOf(w)).ok);
    CHECK(SameText(before, w.Text()));
    CHECK(!ctx.selection.Has());
}

static void TestGameThatCannotStartLeavesTheSceneAlone()
{
    World w;
    EditorContext ctx; ctx.scene = &w.scene;
    ctx.selection.Set(w.arm);
    GameRuntime game; std::vector<std::string> log;
    // the first system changes the scene while starting, the second one fails
    struct Meddler : IGameSystem
    {
        explicit Meddler(World& w) : world(w) {}
        void OnStart(Scene&) override { Wreck(world); }
        void OnUpdate(Scene&, float) override {}
        World& world;
    };
    game.Add(std::make_unique<Meddler>(w));
    AddRecorder(game, "bad", log, 1);

    PlayMode play;
    const std::string before = w.Text();
    const auto r = play.Start(ctx, &game, BridgeOf(w));
    CHECK(!r.ok && r.error.find("bad broke in start") != std::string::npos);
    CHECK(!play.IsPlaying() && !game.IsRunning());
    CHECK(SameText(before, w.Text()));                                    // what the first system did while starting is undone
    CHECK(ctx.selection.Has() && ctx.selection.Get().Get<NameComponent>().name == "Arm");
}

static void TestStopThatCannotRestoreStillEndsThePlay()
{
    World w;
    EditorContext ctx; ctx.scene = &w.scene;
    GameRuntime game;
    PlayMode play;
    CHECK(play.Start(ctx, &game, BridgeOf(w), { w.arm }).ok);
    Wreck(w);
    const std::string wrecked = w.Text();
    const auto r = play.Stop(ctx, &game, nullptr);                        // no bridge: nothing can be put back
    CHECK(!r.ok && !r.error.empty() && !play.IsPlaying());                // the session is over anyway
    CHECK(w.Text() == wrecked);                                           // the scene keeps what the game made of it
    CHECK(play.Start(ctx, &game, BridgeOf(w)).ok);                        // and a new session can begin
    CHECK(play.Stop(ctx, &game, BridgeOf(w)).ok);
}

int main()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "snapshot: restores every component and id",          TestSnapshotRestoresEverything },
        { "snapshot: refuses what it cannot write",             TestSnapshotRefusesWhatItCannotWrite },
        { "snapshot: restore problems",                         TestSnapshotRestoreProblems },
        { "snapshot: pins find entities again by id",           TestSnapshotPins },
        { "game runtime: order, restart",                       TestGameRuntimeOrder },
        { "game runtime: a failing system",                     TestGameRuntimeFailures },
        { "play: stop restores scene, selection, camera, rows", TestPlayThenStopRestoresTheEditor },
        { "play: refused when the scene cannot come back",      TestPlayIsRefusedWhenTheSceneCannotComeBack },
        { "play: without a game",                               TestPlayWithoutGame },
        { "play: a game that cannot start",                     TestGameThatCannotStartLeavesTheSceneAlone },
        { "play: a stop that cannot restore still ends play",   TestStopThatCannotRestoreStillEndsThePlay },
    };
    for (const Test& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll play mode tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
