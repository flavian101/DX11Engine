// UndoHistory without a GPU: steps, undo / redo, the selection and the host's entities by id, refusing what cannot be written
// back, and the limits.
#include "editor/EditorContext.h"
#include "editor/UndoHistory.h"
#include "scene/Scene.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"
#include <cstdio>
#include <map>

using namespace DXEngine;

static int g_failed = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAILED line %d: %s\n", __LINE__, #c); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-5f) { return std::fabs(a - b) <= e; }

// A bridge with one named mesh (no GPU, no files). A mesh that is not in the map has no name a file could use.
struct FakeBridge : IAssetBridge
{
    std::map<std::string, std::shared_ptr<Mesh>> meshes;
    std::shared_ptr<Mesh> AddMesh(const std::string& name)
    {
        auto m = std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("cube", 1.0f)), nullptr);
        meshes[name] = m;
        return m;
    }
    std::string NameMesh(const Mesh& mesh) override { for (auto& [n, m] : meshes) if (m.get() == &mesh) return n; return {}; }
    std::string NameMaterial(const Material&) override { return {}; }
    std::string NameTexture(const RHI::ITexture&, TextureSlot) override { return {}; }
    std::string NameModelOfSkeleton(const Skeleton&) override { return {}; }
    std::shared_ptr<Mesh> ResolveMesh(const std::string& name) override { auto it = meshes.find(name); return it == meshes.end() ? nullptr : it->second; }
    std::shared_ptr<Material> ResolveMaterial(const std::string&) override { return nullptr; }
    std::shared_ptr<RHI::ITexture> ResolveTexture(const std::string&, TextureSlot) override { return nullptr; }
    std::shared_ptr<ModelAsset> ResolveModel(const std::string&) override { return nullptr; }
};

struct World
{
    World()
    {
        mesh = bridge.AddMesh("primitive/cube?size=2");
        rig = scene.CreateEntity("Rig");
        arm = scene.CreateEntity("Arm", rig);
        paint = std::make_shared<Material>("paint", ShadingModel::Lit);
        paint->SetRoughness(0.3f);
        { MeshRendererComponent r; r.mesh = mesh; r.materials = { paint }; arm.Add<MeshRendererComponent>(r); }
        lamp = scene.CreateEntity("Lamp");
        { LightComponent l; l.type = LightType::Point; l.intensity = 300.0f; lamp.Add<LightComponent>(l); }
        TransformSystem::Update(scene);
        ctx.scene = &scene;
    }
    std::string Text() { return SceneSerializer::Save(scene, bridge).text; }
    float RigX() { return scene.FindByName("Rig").Get<TransformComponent>().position.x; }
    void MoveRig(float x) { scene.FindByName("Rig").Get<TransformComponent>().position.x = x; }

    FakeBridge bridge;
    Scene scene;
    EditorContext ctx;
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> paint;
    Entity rig, arm, lamp;
};

static void TestStepsUndoRedo()
{
    World w;
    UndoHistory h;
    CHECK(!h.IsActive() && !h.CanUndo() && !h.CanRedo());
    CHECK(!h.Record(w.scene, w.bridge).ok);                           // no history yet: nothing to record into
    CHECK(!h.Undo(w.ctx, w.bridge).ok && !h.Redo(w.ctx, w.bridge).ok);

    const std::string s0 = w.Text();
    CHECK(h.Reset(w.scene, w.bridge).ok && h.IsActive() && h.Steps() == 1 && !h.CanUndo() && !h.CanRedo());

    const auto same = h.Record(w.scene, w.bridge);                    // nothing changed: no new step
    CHECK(same.ok && !same.changed && h.Steps() == 1);

    w.MoveRig(5.0f);  const std::string s1 = w.Text();
    CHECK(h.Record(w.scene, w.bridge).changed);
    w.MoveRig(9.0f);  const std::string s2 = w.Text();
    w.scene.FindByName("Lamp").Get<LightComponent>().intensity = 10.0f;
    const std::string s2b = w.Text();
    CHECK(h.Record(w.scene, w.bridge).changed);                       // two changes in one gesture are one step
    CHECK(h.Steps() == 3 && h.Position() == 2 && h.CanUndo() && !h.CanRedo() && s2 != s2b);

    CHECK(h.Undo(w.ctx, w.bridge).ok);
    CHECK(w.Text() == s1 && Near(w.RigX(), 5.0f) && h.CanUndo() && h.CanRedo());
    CHECK(h.Undo(w.ctx, w.bridge).ok);
    CHECK(w.Text() == s0 && Near(w.RigX(), 0.0f) && !h.CanUndo());
    CHECK(!h.Undo(w.ctx, w.bridge).ok && w.Text() == s0);              // nothing before the first step
    CHECK(TransformSystem::IsCurrent(w.scene));                        // world matrices are current after a step

    CHECK(h.Redo(w.ctx, w.bridge).ok && w.Text() == s1);
    CHECK(h.Redo(w.ctx, w.bridge).ok && w.Text() == s2b && Near(w.scene.FindByName("Lamp").Get<LightComponent>().intensity, 10.0f));
    CHECK(!h.Redo(w.ctx, w.bridge).ok && !h.CanRedo());
}

static void TestNewEditDropsTheRedoSteps()
{
    World w;
    UndoHistory h;
    h.Reset(w.scene, w.bridge);
    w.MoveRig(1.0f); h.Record(w.scene, w.bridge);
    w.MoveRig(2.0f); h.Record(w.scene, w.bridge);
    h.Undo(w.ctx, w.bridge);
    CHECK(h.CanRedo() && Near(w.RigX(), 1.0f));

    w.MoveRig(7.0f);                                                   // a new edit after an undo
    CHECK(h.Record(w.scene, w.bridge).changed);
    CHECK(!h.CanRedo() && h.Steps() == 3 && h.Position() == 2);        // the "2.0" step is gone
    h.Undo(w.ctx, w.bridge);
    CHECK(Near(w.RigX(), 1.0f));
}

static void TestMaterialValuesAndStructure()
{
    World w;
    UndoHistory h;
    h.Reset(w.scene, w.bridge);

    w.arm.Get<MeshRendererComponent>().materials[0]->SetRoughness(0.9f);          // a scene's own material: its VALUES are part of a step
    h.Record(w.scene, w.bridge);
    Entity extra = w.scene.CreateEntity("Spawned", w.scene.FindByName("Rig"));    // a new entity under the rig
    h.Record(w.scene, w.bridge);
    w.scene.DestroyEntity(w.scene.FindByName("Lamp"));
    h.Record(w.scene, w.bridge);
    CHECK(h.Steps() == 4);

    h.Undo(w.ctx, w.bridge);                                           // the lamp is back, with its light
    CHECK(w.scene.FindByName("Lamp").IsValid() && w.scene.FindByName("Lamp").Has<LightComponent>());
    h.Undo(w.ctx, w.bridge);                                           // the spawned entity is gone again
    CHECK(!w.scene.FindByName("Spawned").IsValid() && w.scene.FindByName("Arm").IsValid());
    h.Undo(w.ctx, w.bridge);
    CHECK(Near(w.scene.FindByName("Arm").Get<MeshRendererComponent>().materials[0]->GetProperties().roughness, 0.3f));
    h.Redo(w.ctx, w.bridge);
    CHECK(Near(w.scene.FindByName("Arm").Get<MeshRendererComponent>().materials[0]->GetProperties().roughness, 0.9f));
}

static void TestSelectionAndKeptEntitiesByIdentity()
{
    World w;
    UndoHistory h;
    h.Reset(w.scene, w.bridge);
    const Uuid armId = w.scene.GetUuid(w.arm), rigId = w.scene.GetUuid(w.rig);

    w.MoveRig(3.0f);
    Entity extra = w.scene.CreateEntity("Spawned");
    h.Record(w.scene, w.bridge);

    // Undo with the selection on an entity that exists in both states, and the host's own entities: one that exists in both, one that
    // was created after the earlier state
    w.ctx.selection.Set(w.scene.FindByName("Arm"));
    auto r = h.Undo(w.ctx, w.bridge, { w.scene.FindByName("Rig"), extra });
    CHECK(r.ok && r.kept.size() == 2);
    CHECK(w.ctx.selection.Has() && w.ctx.selection.Get() == w.scene.FindByUuid(armId));
    CHECK(r.kept[0].IsValid() && r.kept[0] == w.scene.FindByUuid(rigId));
    CHECK(!r.kept[1].IsValid());                                       // "Spawned" is not in the earlier state

    // Redo with the selection on the entity that exists only in the later state: selecting it again after the redo
    r = h.Redo(w.ctx, w.bridge);
    CHECK(r.ok && w.scene.FindByName("Spawned").IsValid());
    w.ctx.selection.Set(w.scene.FindByName("Spawned"));
    h.Undo(w.ctx, w.bridge);
    CHECK(!w.ctx.selection.Has());                                     // the selected entity no longer exists: nothing is selected

    EditorContext none;
    CHECK(!h.Redo(none, w.bridge).ok);                                 // no scene
}

static void TestRefusesWhatItCannotWriteBack()
{
    World w;
    UndoHistory h;
    h.Reset(w.scene, w.bridge);
    w.MoveRig(1.0f); h.Record(w.scene, w.bridge);
    CHECK(h.CanUndo());

    // a mesh made by code with no name a file could use: an undo could not bring it back, so there is no history rather than a lossy one
    Entity stray = w.scene.CreateEntity("Stray");
    { MeshRendererComponent r; r.mesh = std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("x", 1.0f)), nullptr); stray.Add<MeshRendererComponent>(r); }
    const auto refused = h.Record(w.scene, w.bridge);
    CHECK(!refused.ok && refused.warnings.size() == 1 && refused.warnings[0].find("Stray") != std::string::npos);
    CHECK(!h.IsActive() && !h.CanUndo() && h.Steps() == 0);
    const std::string kept = w.Text();
    CHECK(!h.Undo(w.ctx, w.bridge).ok && w.Text() == kept);            // nothing happens to the scene

    CHECK(!h.Reset(w.scene, w.bridge).ok && !h.IsActive());            // still not writable
    w.scene.DestroyEntity(w.scene.FindByName("Stray"));
    CHECK(h.Reset(w.scene, w.bridge).ok && h.IsActive() && h.Steps() == 1);   // writable again: a new history
}

static void TestLimits()
{
    {
        World w;
        UndoHistory::Limits limits; limits.maxSteps = 3;
        UndoHistory h(limits);
        h.Reset(w.scene, w.bridge);
        for (int i = 1; i <= 6; ++i) { w.MoveRig(static_cast<float>(i)); CHECK(h.Record(w.scene, w.bridge).changed); }
        CHECK(h.Steps() == 3 && h.Position() == 2);                    // the three newest: 4, 5, 6
        CHECK(h.Undo(w.ctx, w.bridge).ok && Near(w.RigX(), 5.0f));
        CHECK(h.Undo(w.ctx, w.bridge).ok && Near(w.RigX(), 4.0f));
        CHECK(!h.CanUndo());                                           // 3 was dropped
    }
    {
        World w;
        UndoHistory::Limits limits; limits.maxBytes = 1;                // far less than one state: the newest still stays
        UndoHistory h(limits);
        h.Reset(w.scene, w.bridge);
        w.MoveRig(1.0f);
        CHECK(h.Record(w.scene, w.bridge).changed);
        CHECK(h.Steps() == 1 && !h.CanUndo() && h.Bytes() == w.Text().size());
        CHECK(Near(w.RigX(), 1.0f));
    }
    {
        World w;                                                       // the byte count follows the steps
        UndoHistory h;
        h.Reset(w.scene, w.bridge);
        const size_t one = h.Bytes();
        w.MoveRig(1.0f); h.Record(w.scene, w.bridge);
        CHECK(h.Bytes() > one);
        h.Undo(w.ctx, w.bridge);
        w.MoveRig(2.0f); h.Record(w.scene, w.bridge);                  // replaces the undone step
        CHECK(h.Steps() == 2 && h.Bytes() >= one && h.Bytes() < 3 * one);
        h.Clear();
        CHECK(!h.IsActive() && h.Bytes() == 0);
    }
}

int main()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "steps: record, undo, redo",                        TestStepsUndoRedo },
        { "a new edit drops the redo steps",                  TestNewEditDropsTheRedoSteps },
        { "material values and structure",                    TestMaterialValuesAndStructure },
        { "selection and kept entities by id",                TestSelectionAndKeptEntitiesByIdentity },
        { "refuses what it cannot write back",                TestRefusesWhatItCannotWriteBack },
        { "limits: steps and bytes",                          TestLimits },
    };
    for (const Test& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll undo history tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
