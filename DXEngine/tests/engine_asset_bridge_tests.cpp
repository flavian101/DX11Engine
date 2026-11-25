// EngineAssetBridge: naming and finding assets with the model library and importer, with no device (generated meshes work
// without one; texture loading does not, and says so). Ends with a scene saved through one bridge and loaded through a fresh one.
#include "Animation/AnimationClip.h"
#include "assets/EngineAssetBridge.h"
#include "models/ModelAsset.h"
#include "scene/Components.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include "utils/AssetNames.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace DXEngine;
namespace fs = std::filesystem;

static int g_failed = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAILED line %d: %s\n", __LINE__, #c); ++g_failed; } } while (0)

struct FakeTexture : RHI::ITexture
{
    explicit FakeTexture(std::string n) : name(std::move(n)) {}
    std::string name;
    const std::string& GetDebugName() const override { return name; }
    void SetDebugName(const std::string& n) override { name = n; }
    uint64_t GetGPUHandle() const override { return 0; }
    size_t GetMemoryUsage() const override { return 0; }
    bool IsValid() const override { return true; }
    RHI::TextureType GetType() const override { return RHI::TextureType{}; }
    RHI::TextureFormat GetFormat() const override { return RHI::TextureFormat{}; }
    uint32_t GetWidth() const override { return 1; }
    uint32_t GetHeight() const override { return 1; }
    uint32_t GetDepth() const override { return 1; }
    uint32_t GetMipLevels() const override { return 1; }
    uint32_t GetArraySize() const override { return 1; }
    uint32_t GetSampleCount() const override { return 1; }
    bool Update(const void*, uint32_t, uint32_t) override { return true; }
    bool GenerateMips() override { return true; }
    bool ReadPixels(void*, uint32_t, uint32_t) override { return true; }
    void* GetNativeHandle() const override { return nullptr; }
};

static std::shared_ptr<Skeleton> TwoBoneSkeleton()
{
    auto skeleton = std::make_shared<Skeleton>();
    Bone root;  root.Name = "root";
    Bone child; child.Name = "child"; child.ParentIndex = 0;
    skeleton->AddBone(root);
    skeleton->AddBone(child);
    return skeleton;
}

// A "file reader" that makes a model with two meshes, two materials, a skeleton and a clip, and counts its calls.
struct Fixture
{
    Fixture()
    {
        root = (fs::temp_directory_path() / "dx_bridge_tests").generic_string();
        fs::remove_all(root);
        fs::create_directories(root + "/models");
        fs::create_directories(root + "/textures");
        for (const char* f : { "models/horse.fbx", "textures/a.png", "textures/b.png", "textures/c.png", "textures/d.png", "textures/e.png", "textures/f.png" })
            std::ofstream(root + "/" + f) << "x";
        importer.SetLoadFunction([this](const std::string& path, bool, std::string& error) -> std::shared_ptr<ModelAsset>
        {
            ++imports;
            if (path.find("broken") != std::string::npos) { error = "cannot read"; return nullptr; }
            auto model = std::make_shared<ModelAsset>();
            model->name = "horse"; model->sourcePath = path;
            model->AddNode("root", -1, TransformComponent::Identity());
            model->AddMesh("body", Mesh::CreateCube(nullptr, 1.0f));
            model->AddMesh("tail", Mesh::CreateSphere(nullptr, 1.0f, 8));
            model->meshes[0].mesh->SetSourceName({});                  // a model's meshes are not "primitives": the library names them
            model->meshes[1].mesh->SetSourceName({});
            model->materials = { std::make_shared<Material>("coat", ShadingModel::Lit), std::make_shared<Material>("mane", ShadingModel::Lit) };
            model->skeleton = TwoBoneSkeleton();
            model->clips = { std::make_shared<AnimationClip>("walk", 1.0f) };
            return model;
        });
    }
    ~Fixture() { fs::remove_all(root); }

    EngineAssetBridge Bridge() { return EngineAssetBridge(nullptr, library, importer, log, root); }
    size_t Warnings() const { return log.Count(Assets::LogSeverity::Warning); }

    std::string root;
    ModelLibrary library;
    ModelImporter importer;
    Assets::ImportLog log;
    int imports = 0;
};

static void TestGeneratedMeshes()
{
    Fixture f; auto bridge = f.Bridge();
    auto cube = Mesh::CreateCube(nullptr, 2.0f);
    auto sphere = Mesh::CreateSphere(nullptr, 0.5f, 16);
    auto plane = Mesh::CreatePlane(nullptr, 10.0f, 5.0f, 4, 2);
    auto quad = Mesh::CreateQuad(nullptr, 1.0f, 3.0f);
    CHECK(bridge.NameMesh(*cube) == "primitive/cube?size=2" && bridge.NameMesh(*sphere) == "primitive/sphere?radius=0.5&segments=16");
    CHECK(bridge.NameMesh(*plane) == "primitive/plane?width=10&depth=5&widthSegments=4&depthSegments=2" && bridge.NameMesh(*quad) == "primitive/quad?width=1&height=3");

    for (const auto& original : { cube, sphere, plane, quad })
    {
        auto again = bridge.ResolveMesh(bridge.NameMesh(*original));
        CHECK(again && again != original && again->GetSourceName() == original->GetSourceName());       // a new mesh, same name
        CHECK(again && again->GetResource()->GetVertexData()->GetVertexCount() == original->GetResource()->GetVertexData()->GetVertexCount());   // the same shape
    }
    CHECK(f.imports == 0 && f.Warnings() == 0);

    CHECK(!bridge.ResolveMesh("primitive/sphere?radius=1&segments=999999") && f.Warnings() == 1);   // a silly name: refused, said so
    CHECK(!bridge.ResolveMesh("something/else") && f.Warnings() == 2);

    auto stranger = std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("c", 1.0f)), nullptr);
    CHECK(bridge.NameMesh(*stranger).empty());                        // not generated, not in a model: no name
}

static void TestModelParts()
{
    Fixture f; auto bridge = f.Bridge();
    // not imported yet: resolving imports (once), the log says what happened
    auto body = bridge.ResolveMesh("model/models/horse.fbx#mesh/0");
    CHECK(body && f.imports == 1 && f.library.Count() == 1);
    auto tail = bridge.ResolveMesh("model/models/horse.fbx#mesh/1");
    CHECK(tail && tail != body && f.imports == 1);                    // the library has it now
    auto coat = bridge.ResolveMaterial("model/models/horse.fbx#material/0");
    CHECK(coat && coat->GetName() == "coat" && f.imports == 1);
    auto model = bridge.ResolveModel("models/horse.fbx");
    CHECK(model && model->meshes[0].mesh == body && model->materials[0] == coat && f.imports == 1);

    // and the other way
    CHECK(bridge.NameMesh(*body) == "model/models/horse.fbx#mesh/0" && bridge.NameMesh(*tail) == "model/models/horse.fbx#mesh/1");
    CHECK(bridge.NameMaterial(*coat) == "model/models/horse.fbx#material/0" && bridge.NameMaterial(*model->materials[1]) == "model/models/horse.fbx#material/1");
    CHECK(bridge.NameModelOfSkeleton(*model->skeleton) == "models/horse.fbx");
    CHECK(bridge.NameModelOfSkeleton(*TwoBoneSkeleton()).empty());
    CHECK(bridge.NameMaterial(Material("mine", ShadingModel::Lit)).empty());          // yours: saved by value, no name

    // what does not exist
    const size_t before = f.Warnings();
    CHECK(!bridge.ResolveMesh("model/models/horse.fbx#mesh/7") && !bridge.ResolveMaterial("model/models/horse.fbx#material/9"));
    CHECK(f.Warnings() == before + 2 && f.imports == 1);
    CHECK(!bridge.ResolveMesh("model/models/broken.fbx#mesh/0") && f.imports == 2);   // the import failed: null, and the importer's own log line
    CHECK(!bridge.ResolveModel("models/broken.fbx"));
    CHECK(!bridge.ResolveMaterial("primitive/cube?size=1"));                          // wrong kind of name
    CHECK(!bridge.ResolveMesh("model/models/horse.fbx#material/0"));
}

static void TestTextureNames()
{
    Fixture f; auto bridge = f.Bridge();
    const std::string full = f.root + "/textures/a.png";
    FakeTexture byFullPath(full), byStored("textures/b.png"), missing(f.root + "/textures/gone.png"), solid("SolidColor"), embedded("horse.fbx::*0"), empty("");
    CHECK(bridge.NameTexture(byFullPath, TextureSlot::Diffuse) == "textures/a.png");     // under the root: relative
    CHECK(bridge.NameTexture(byStored, TextureSlot::Diffuse) == "textures/b.png");       // already relative to the root
    CHECK(bridge.NameTexture(missing, TextureSlot::Diffuse).empty());                    // a file that is gone
    CHECK(bridge.NameTexture(solid, TextureSlot::Diffuse).empty() && bridge.NameTexture(embedded, TextureSlot::Diffuse).empty() && bridge.NameTexture(empty, TextureSlot::Diffuse).empty());

    const std::array<std::string, 6> faces = { f.root + "/textures/a.png", f.root + "/textures/b.png", f.root + "/textures/c.png",
                                               f.root + "/textures/d.png", f.root + "/textures/e.png", f.root + "/textures/f.png" };
    FakeTexture cube(AssetNames::CubeMapName(faces));
    CHECK(bridge.NameTexture(cube, TextureSlot::Environment) == "cube:textures/a.png|textures/b.png|textures/c.png|textures/d.png|textures/e.png|textures/f.png");
    std::array<std::string, 6> broken = faces; broken[3] = f.root + "/textures/gone.png";
    FakeTexture brokenCube(AssetNames::CubeMapName(broken));
    CHECK(bridge.NameTexture(brokenCube, TextureSlot::Environment).empty());             // one face is gone

    // no device: loading says so instead of crashing
    CHECK(!bridge.ResolveTexture("textures/a.png", TextureSlot::Diffuse));
    CHECK(!bridge.ResolveTexture("cube:a|b|c|d|e|f", TextureSlot::Environment));
    CHECK(f.Warnings() == 2);
}

static void TestSceneThroughTwoBridges()
{
    Fixture f1;
    auto bridge1 = f1.Bridge();
    auto model = bridge1.ResolveModel("models/horse.fbx");
    CHECK(model != nullptr);

    Scene scene;
    Entity horse = scene.CreateEntity("horse");
    { AnimatorComponent a; a.skeleton = model->skeleton; a.clips = model->clips; a.clip = 0; a.speed = 2.0f; horse.Add<AnimatorComponent>(a); }
    Entity body = scene.CreateEntity("body", horse);
    { MeshRendererComponent r; r.mesh = model->meshes[0].mesh; r.materials = { model->materials[0] }; body.Add<MeshRendererComponent>(r); body.Add<SkinnedMeshComponent>(); }
    Entity cube = scene.CreateEntity("cube");
    auto cubeMesh = Mesh::CreateCube(nullptr, 3.0f);
    auto own = std::make_shared<Material>("my paint", ShadingModel::Lit);
    own->SetRoughness(0.25f);
    { MeshRendererComponent r; r.mesh = cubeMesh; r.materials = { own }; cube.Add<MeshRendererComponent>(r); }
    Entity ball = scene.CreateEntity("ball");
    { MeshRendererComponent r; r.mesh = Mesh::CreateSphere(nullptr, 0.5f, 12); ball.Add<MeshRendererComponent>(r); }

    const auto saved = SceneSerializer::Save(scene, bridge1);
    CHECK(saved.warnings.empty());
    CHECK(saved.text.find("model/models/horse.fbx#mesh/0") != std::string::npos);
    CHECK(saved.text.find("\"model\": \"models/horse.fbx\"") != std::string::npos);
    CHECK(saved.text.find(f1.root) == std::string::npos);                     // no machine-specific path in the file

    // a fresh session: nothing imported yet, the file's names pull in what they need
    Fixture f2;
    auto bridge2 = f2.Bridge();
    Scene loaded;
    const auto r = SceneSerializer::Load(saved.text, loaded, bridge2);
    CHECK(r.ok && r.warnings.empty() && loaded.EntityCount() == 4);
    CHECK(f2.imports == 1 && f2.library.Count() == 1);                         // imported ONCE for the animator, the mesh and the material
    auto lmodel = bridge2.ResolveModel("models/horse.fbx");
    const AnimatorComponent* la = loaded.FindByUuid(scene.GetUuid(horse)).TryGet<AnimatorComponent>();
    CHECK(la && la->skeleton == lmodel->skeleton && la->speed == 2.0f && la->clips.size() == 1);
    const MeshRendererComponent* lb = loaded.FindByUuid(scene.GetUuid(body)).TryGet<MeshRendererComponent>();
    CHECK(lb && lb->mesh == lmodel->meshes[0].mesh && lb->materials.size() == 1 && lb->materials[0] == lmodel->materials[0]);
    CHECK(loaded.FindByUuid(scene.GetUuid(body)).Has<SkinnedMeshComponent>());
    const MeshRendererComponent* lc = loaded.FindByUuid(scene.GetUuid(cube)).TryGet<MeshRendererComponent>();
    CHECK(lc && lc->mesh->GetSourceName() == "primitive/cube?size=3" && lc->materials[0]->GetName() == "my paint" && lc->materials[0]->GetProperties().roughness == 0.25f);
    CHECK(loaded.FindByUuid(scene.GetUuid(ball)).TryGet<MeshRendererComponent>()->mesh->GetSourceName() == "primitive/sphere?radius=0.5&segments=12");

    // and it saves back to the same text
    CHECK(SceneSerializer::Save(loaded, bridge2).text == saved.text);

    // a model that is gone: the entities stay, the attachments that needed it are dropped, and the log says so
    Fixture f3;
    f3.importer.SetLoadFunction([](const std::string&, bool, std::string& error) -> std::shared_ptr<ModelAsset> { error = "file not found"; return nullptr; });
    auto bridge3 = f3.Bridge();
    Scene noModel;
    const auto r3 = SceneSerializer::Load(saved.text, noModel, bridge3);
    CHECK(r3.ok && noModel.EntityCount() == 4 && !r3.warnings.empty());
    CHECK(!noModel.FindByUuid(scene.GetUuid(horse)).Has<AnimatorComponent>() && !noModel.FindByUuid(scene.GetUuid(body)).Has<MeshRendererComponent>());
    CHECK(noModel.FindByUuid(scene.GetUuid(cube)).Has<MeshRendererComponent>());           // generated meshes need no file
}

// Reuse: a scene about to be replaced hands out the textures it holds, so putting a snapshot back (play mode's Stop) does not decode
// the same files again. With no device loading is impossible, so a texture that comes back proves it came from the old scene.
static void TestReuseTexturesOfALiveScene()
{
    Fixture f; auto bridge = f.Bridge();

    Scene scene;
    auto wall = std::make_shared<FakeTexture>(f.root + "/textures/a.png");           // the same file bound to a colour slot ...
    auto wallData = std::make_shared<FakeTexture>(f.root + "/textures/a.png");       // ... and to a data slot: another texture (other format)
    auto solid = std::make_shared<FakeTexture>("SolidColor");                         // not a file
    auto paint = std::make_shared<Material>("paint", ShadingModel::Lit);
    paint->SetTexture(TextureSlot::Diffuse, wall);
    paint->SetTexture(TextureSlot::Normal, wallData);
    paint->SetTexture(TextureSlot::Emissive, solid);
    Entity cube = scene.CreateEntity("cube");
    { MeshRendererComponent r; r.mesh = Mesh::CreateCube(nullptr, 2.0f); r.materials = { paint }; cube.Add<MeshRendererComponent>(r); }

    CHECK(!bridge.ResolveTexture("textures/a.png", TextureSlot::Diffuse));          // before: no device, nothing to give
    const size_t warningsBefore = f.Warnings();

    bridge.Reuse(scene);
    CHECK(bridge.ResolveTexture("textures/a.png", TextureSlot::Diffuse) == wall);
    CHECK(bridge.ResolveTexture("textures/a.png", TextureSlot::Normal) == wallData);   // the colour space belongs to the slot
    CHECK(!bridge.ResolveTexture("SolidColor", TextureSlot::Emissive));                // only what a file can name is remembered
    CHECK(f.Warnings() == warningsBefore + 1);                                         // (the solid colour's lookup warned, the others did not)

    // the whole circle: save, take the textures of the live scene, load over it: the new scene shows the very same texture objects
    const auto saved = SceneSerializer::Save(scene, bridge);
    CHECK(saved.warnings.size() == 1);                                                 // the solid colour has no file: it was not saved
    Fixture f2; auto fresh = f2.Bridge();
    fresh.Reuse(scene);
    Scene loaded;
    const auto r = SceneSerializer::Load(saved.text, loaded, fresh);
    CHECK(r.ok);
    const MeshRendererComponent* lr = loaded.FindByUuid(scene.GetUuid(cube)).TryGet<MeshRendererComponent>();
    CHECK(lr && lr->materials.size() == 1);
    if (lr && lr->materials.size() == 1)
    {
        CHECK(lr->materials[0]->GetTexture(TextureSlot::Diffuse) == wall);
        CHECK(lr->materials[0]->GetTexture(TextureSlot::Normal) == wallData);
        CHECK(!lr->materials[0]->GetTexture(TextureSlot::Emissive));
    }
}

int main()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "generated meshes: name and rebuild",           TestGeneratedMeshes },
        { "model parts: import on demand, both ways",     TestModelParts },
        { "texture names (files, cube maps, no device)",  TestTextureNames },
        { "a scene through two bridges",                  TestSceneThroughTwoBridges },
        { "reuse: a live scene hands out its textures",   TestReuseTexturesOfALiveScene },
    };
    for (const Test& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll engine asset bridge tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
