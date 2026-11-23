// SceneSerializer: scene <-> text, with a fake asset bridge (no GPU, no files).
#include "models/ModelAsset.h"
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include "scene/TransformSystem.h"
#include "utils/Json.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"
#include "utils/material/MaterialSlots.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

using namespace DXEngine;

static int g_failed = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAILED line %d: %s\n", __LINE__, #c); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-6f) { return std::fabs(a - b) <= e; }

// ------------------------------------------------------------------------------------------------ fixtures
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

struct FakeBridge : IAssetBridge
{
    std::map<std::string, std::shared_ptr<Mesh>>     meshes;       // names a file can use
    std::map<std::string, std::shared_ptr<Material>> materials;    // "model materials": saved by name
    std::map<std::string, std::shared_ptr<ModelAsset>> models;
    std::map<std::string, std::shared_ptr<FakeTexture>> textures;  // files that exist
    std::set<std::string> unloadable;                              // names whose loading fails
    int meshResolves = 0, textureResolves = 0;

    std::shared_ptr<Mesh> AddMesh(const std::string& name)
    {
        auto m = std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("cube", 1.0f)), nullptr);
        meshes[name] = m;
        return m;
    }

    std::string NameMesh(const Mesh& mesh) override
    {
        for (auto& [name, m] : meshes) if (m.get() == &mesh) return name;
        return {};
    }
    std::string NameMaterial(const Material& material) override
    {
        for (auto& [name, m] : materials) if (m.get() == &material) return name;
        return {};
    }
    std::string NameTexture(const RHI::ITexture& texture, TextureSlot) override
    {
        for (auto& [name, t] : textures) if (t.get() == &texture) return name;
        return {};
    }
    std::string NameModelOfSkeleton(const Skeleton& skeleton) override
    {
        for (auto& [name, m] : models) if (m->skeleton.get() == &skeleton) return name;
        return {};
    }

    std::shared_ptr<Mesh> ResolveMesh(const std::string& name) override
    {
        ++meshResolves;
        const auto it = meshes.find(name);
        return (it != meshes.end() && !unloadable.count(name)) ? it->second : nullptr;
    }
    std::shared_ptr<Material> ResolveMaterial(const std::string& name) override
    {
        const auto it = materials.find(name);
        return (it != materials.end() && !unloadable.count(name)) ? it->second : nullptr;
    }
    std::shared_ptr<RHI::ITexture> ResolveTexture(const std::string& name, TextureSlot) override
    {
        ++textureResolves;
        const auto it = textures.find(name);
        return (it != textures.end() && !unloadable.count(name)) ? it->second : nullptr;
    }
    std::shared_ptr<ModelAsset> ResolveModel(const std::string& name) override
    {
        const auto it = models.find(name);
        return (it != models.end() && !unloadable.count(name)) ? it->second : nullptr;
    }
};

static bool HasWarning(const std::vector<std::string>& w, const char* part)
{
    for (const auto& s : w) if (s.find(part) != std::string::npos) return true;
    return false;
}

// Equal text; when not, shows the first line that differs (a failure that says only "false" costs a debugging session).
static bool SameText(const std::string& a, const std::string& b)
{
    if (a == b) return true;
    size_t i = 0, line = 1;
    while (i < a.size() && i < b.size() && a[i] == b[i]) { if (a[i] == '\n') ++line; ++i; }
    const auto excerpt = [&](const std::string& t) { size_t s = t.rfind('\n', i == 0 ? 0 : i - 1); s = (s == std::string::npos) ? 0 : s + 1; return t.substr(s, t.find('\n', i) - s); };
    std::printf("    texts differ at line %zu:\n      was:  %s\n      now:  %s\n", line, excerpt(a).c_str(), excerpt(b).c_str());
    return false;
}

static SceneSerializer::LoadResult Load(const std::string& text, Scene& scene, FakeBridge& bridge)
{
    return SceneSerializer::Load(text, scene, bridge);
}

// ------------------------------------------------------------------------------------------------ tests
static void TestEmptyScene()
{
    FakeBridge bridge;
    Scene scene;
    const auto saved = SceneSerializer::Save(scene, bridge);
    CHECK(saved.warnings.empty());
    Scene other;
    other.CreateEntity("old");
    const auto r = Load(saved.text, other, bridge);
    CHECK(r.ok && r.entities == 0 && other.EntityCount() == 0);      // loading REPLACES
}

static void TestHierarchyAndTransforms()
{
    FakeBridge bridge;
    Scene scene;
    Entity root = scene.CreateEntity("root");
    Entity a = scene.CreateEntity("a", root);
    Entity b = scene.CreateEntity("b", root);
    Entity c = scene.CreateEntity("c", root);
    Entity grand = scene.CreateEntity("grand", b);
    Entity other = scene.CreateEntity("other root");
    root.Get<TransformComponent>().position = { 1.5f, -2.0f, 0.1f };
    b.Get<TransformComponent>().scale = { 2.0f, 3.0f, 0.5f };
    b.Get<TransformComponent>().SetEulerDegrees(10.0f, 20.0f, 30.0f);
    grand.Get<TransformComponent>().position = { 1.0f / 3.0f, 1e-7f, 123456.789f };

    // reorder siblings: c first
    scene.SetParent(c, Entity());
    scene.SetParent(c, root);
    const Uuid idRoot = scene.GetUuid(root), idA = scene.GetUuid(a), idB = scene.GetUuid(b), idC = scene.GetUuid(c), idGrand = scene.GetUuid(grand);
    const TransformComponent gt = grand.Get<TransformComponent>(), bt = b.Get<TransformComponent>();

    const auto saved = SceneSerializer::Save(scene, bridge);
    CHECK(saved.warnings.empty());

    Scene loaded;
    const auto r = Load(saved.text, loaded, bridge);
    CHECK(r.ok && r.warnings.empty() && r.entities == 6 && loaded.EntityCount() == 6);

    // identity survives: the same ids name the same things
    Entity lroot = loaded.FindByUuid(idRoot), la = loaded.FindByUuid(idA), lb = loaded.FindByUuid(idB), lc = loaded.FindByUuid(idC), lgrand = loaded.FindByUuid(idGrand);
    CHECK(lroot.IsValid() && la.IsValid() && lb.IsValid() && lc.IsValid() && lgrand.IsValid());
    CHECK(lroot.Get<NameComponent>().name == "root" && lgrand.Get<NameComponent>().name == "grand");
    CHECK(loaded.GetParent(la) == lroot && loaded.GetParent(lgrand) == lb && !loaded.GetParent(lroot).IsValid());

    // sibling order is part of the file: a, b came before c was re-attached, so the order is a, b, c
    const auto kids = loaded.GetChildren(lroot);
    CHECK(kids.size() == 3 && kids[0] == la && kids[1] == lb && kids[2] == lc);
    const auto original = scene.GetChildren(root);
    CHECK(original.size() == 3 && original[2] == c);

    // transforms come back bit for bit
    const TransformComponent& lgt = lgrand.Get<TransformComponent>();
    CHECK(lgt.position.x == gt.position.x && lgt.position.y == gt.position.y && lgt.position.z == gt.position.z);
    const TransformComponent& lbt = lb.Get<TransformComponent>();
    CHECK(Near(lbt.rotation.x, bt.rotation.x, 1e-6f) && Near(lbt.rotation.y, bt.rotation.y, 1e-6f) && Near(lbt.rotation.z, bt.rotation.z, 1e-6f) && Near(lbt.rotation.w, bt.rotation.w, 1e-6f));
    CHECK(lbt.scale.x == 2.0f && lbt.scale.y == 3.0f && lbt.scale.z == 0.5f);

    // saving what was loaded gives the same text (nothing drifts)
    const auto again = SceneSerializer::Save(loaded, bridge);
    CHECK(SameText(saved.text, again.text));

    // world matrices are derived: not current until the system runs
    CHECK(!TransformSystem::IsCurrent(loaded));
    TransformSystem::Update(loaded);
    CHECK(TransformSystem::IsCurrent(loaded));
    TransformSystem::Update(scene);
    CHECK(Near(lgrand.Get<TransformComponent>().world._41, grand.Get<TransformComponent>().world._41, 1e-3f));
    (void)other;
}

static void TestComponents()
{
    FakeBridge bridge;
    auto cube = bridge.AddMesh("primitive/cube?size=1");
    auto sphere = bridge.AddMesh("primitive/sphere?radius=1");
    auto modelMaterial = std::make_shared<Material>("from model", ShadingModel::Lit);
    bridge.materials["model/horse#material/0"] = modelMaterial;

    Scene scene;
    Entity sun = scene.CreateEntity("sun");
    LightComponent light; light.type = LightType::Spot; light.color = { 0.9f, 0.8f, 0.7f }; light.intensity = 4.5f; light.range = 33.0f;
    light.innerConeDegrees = 12.0f; light.outerConeDegrees = 27.5f; light.castsShadows = true;
    sun.Add<LightComponent>(light);
    Entity sky = scene.CreateEntity("sky"); sky.Add<SkyboxComponent>();
    Entity skinned = scene.CreateEntity("skinned"); skinned.Add<SkinnedMeshComponent>();

    auto shared = std::make_shared<Material>("shared inline", ShadingModel::Lit);
    shared->SetDiffuseColor({ 0.25f, 0.5f, 0.75f, 0.9f });
    shared->SetMetallic(0.8f); shared->SetRoughness(0.3f); shared->SetEmissiveIntensity(7.0f); shared->SetNormalScale(1.5f);
    shared->SetTextureScale({ 4.0f, 2.0f }); shared->SetTextureOffset({ 0.25f, 0.5f });
    shared->SetDataChannel(TextureSlot::Roughness, 1); shared->SetDataChannel(TextureSlot::Metallic, 2); shared->SetNormalFlipY(true);
    RenderState state = RenderState::Blended(); state.cull = RHI::CullMode::None; state.queue = RenderQueue::Transparent;
    shared->SetRenderState(state);
    bridge.textures["textures/grass.jpg"] = std::make_shared<FakeTexture>("grass");
    bridge.textures["textures/grass_n.jpg"] = std::make_shared<FakeTexture>("grass_n");
    shared->SetTexture(TextureSlot::Diffuse, bridge.textures["textures/grass.jpg"]);
    shared->SetTexture(TextureSlot::Normal, bridge.textures["textures/grass_n.jpg"]);

    Entity one = scene.CreateEntity("one");
    { MeshRendererComponent r; r.mesh = cube; r.materials = { shared, modelMaterial, nullptr }; r.castsShadows = false; r.receivesShadows = false; r.visible = false; one.Add<MeshRendererComponent>(r); }
    Entity two = scene.CreateEntity("two");
    { MeshRendererComponent r; r.mesh = cube; r.materials = { shared }; two.Add<MeshRendererComponent>(r); }
    Entity three = scene.CreateEntity("three");
    { MeshRendererComponent r; r.mesh = sphere; three.Add<MeshRendererComponent>(r); }
    const Uuid idOne = scene.GetUuid(one), idTwo = scene.GetUuid(two), idThree = scene.GetUuid(three);

    const auto saved = SceneSerializer::Save(scene, bridge);
    CHECK(saved.warnings.empty());

    Scene loaded;
    bridge.meshResolves = bridge.textureResolves = 0;
    const auto r = Load(saved.text, loaded, bridge);
    CHECK(r.ok && r.warnings.empty());

    Entity lsun = loaded.FindByUuid(scene.GetUuid(sun));
    const LightComponent* l = lsun.TryGet<LightComponent>();
    CHECK(l && l->type == LightType::Spot && l->color.x == 0.9f && l->color.y == 0.8f && l->color.z == 0.7f && l->intensity == 4.5f && l->range == 33.0f);
    CHECK(l && l->innerConeDegrees == 12.0f && l->outerConeDegrees == 27.5f && l->castsShadows);
    CHECK(loaded.FindByUuid(scene.GetUuid(sky)).Has<SkyboxComponent>() && !loaded.FindByUuid(scene.GetUuid(sky)).Has<LightComponent>());
    CHECK(loaded.FindByUuid(scene.GetUuid(skinned)).Has<SkinnedMeshComponent>());

    Entity lone = loaded.FindByUuid(idOne), ltwo = loaded.FindByUuid(idTwo), lthree = loaded.FindByUuid(idThree);
    const MeshRendererComponent* r1 = lone.TryGet<MeshRendererComponent>();
    const MeshRendererComponent* r2 = ltwo.TryGet<MeshRendererComponent>();
    const MeshRendererComponent* r3 = lthree.TryGet<MeshRendererComponent>();
    CHECK(r1 && r2 && r3);
    CHECK(r1->mesh == cube && r2->mesh == cube && r3->mesh == sphere);              // the bridge's objects, shared again
    CHECK(!r1->visible && !r1->castsShadows && !r1->receivesShadows && r2->visible && r2->castsShadows && r2->receivesShadows);
    CHECK(r1->materials.size() == 3 && r1->materials[1] == modelMaterial && r1->materials[2] == nullptr);   // a named material, "the mesh's own"
    CHECK(r2->materials.size() == 1 && r3->materials.empty());
    CHECK(r1->materials[0] == r2->materials[0] && r1->materials[0] != shared);       // an inline material is written ONCE and shared again
    CHECK(bridge.meshResolves == 2);                                                // one resolve per distinct name

    const Material& m = *r1->materials[0];
    const MaterialProperties& p = m.GetProperties();
    CHECK(m.GetName() == "shared inline" && m.GetShadingModel() == ShadingModel::Lit);
    CHECK(p.diffuseColor.x == 0.25f && p.diffuseColor.y == 0.5f && p.diffuseColor.z == 0.75f && p.diffuseColor.w == 0.9f);
    CHECK(p.metallic == 0.8f && p.roughness == 0.3f && p.emissiveIntensity == 7.0f && p.normalScale == 1.5f);
    CHECK(p.textureScale.x == 4.0f && p.textureScale.y == 2.0f && p.textureOffset.x == 0.25f && p.textureOffset.y == 0.5f);
    CHECK(m.GetDataChannel(TextureSlot::Roughness) == 1 && m.GetDataChannel(TextureSlot::Metallic) == 2 && p.dataChannels.w == 1.0f);
    CHECK(m.GetRenderState() == shared->GetRenderState());
    CHECK(m.GetTextures()[static_cast<size_t>(TextureSlot::Diffuse)] == bridge.textures["textures/grass.jpg"]);
    CHECK(m.GetTextures()[static_cast<size_t>(TextureSlot::Normal)] == bridge.textures["textures/grass_n.jpg"]);
    CHECK(m.GetTextures()[static_cast<size_t>(TextureSlot::Emissive)] == nullptr);
    CHECK(m.GetTextureMask() == shared->GetTextureMask());                           // the shader variant would be the same

    CHECK(SameText(saved.text, SceneSerializer::Save(loaded, bridge).text));
}

static void TestAnimator()
{
    FakeBridge bridge;
    auto model = std::make_shared<ModelAsset>();
    model->skeleton = TwoBoneSkeleton();
    model->clips = { std::make_shared<AnimationClip>("idle", 1.0f), std::make_shared<AnimationClip>("walk", 2.0f) };
    bridge.models["models/horse.fbx"] = model;

    Scene scene;
    Entity root = scene.CreateEntity("horse");
    AnimatorComponent a; a.skeleton = model->skeleton; a.clips = model->clips; a.clip = 1; a.speed = -0.5f; a.mode = PlaybackMode::PingPong; a.playing = false; a.time = 0.75f;
    root.Add<AnimatorComponent>(a);
    root.Get<AnimatorComponent>().boneMatrices.assign(2, TransformComponent::Identity());      // output: not saved

    const auto saved = SceneSerializer::Save(scene, bridge);
    CHECK(saved.warnings.empty());
    CHECK(saved.text.find("boneMatrices") == std::string::npos);

    Scene loaded;
    const auto r = Load(saved.text, loaded, bridge);
    CHECK(r.ok && r.warnings.empty());
    const AnimatorComponent* la = loaded.FindByUuid(scene.GetUuid(root)).TryGet<AnimatorComponent>();
    CHECK(la != nullptr);
    CHECK(la && la->skeleton == model->skeleton && la->clips.size() == 2 && la->clips[1] == model->clips[1]);   // shared with the model, not copies
    CHECK(la && la->clip == 1 && la->speed == -0.5f && la->mode == PlaybackMode::PingPong && !la->playing && la->time == 0.75f);
    CHECK(la && la->boneMatrices.empty());                                           // the system fills them in
}

static void TestSaveWarnings()
{
    FakeBridge bridge;
    Scene scene;
    // a mesh the bridge has no name for
    Entity a = scene.CreateEntity("nameless mesh");
    { MeshRendererComponent r; r.mesh = std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("c", 1.0f)), nullptr); a.Add<MeshRendererComponent>(r); }
    // a renderer without a mesh
    Entity b = scene.CreateEntity("no mesh");
    b.Add<MeshRendererComponent>(MeshRendererComponent{});
    // an inline material with a texture that has no file
    auto cube = bridge.AddMesh("primitive/cube");
    auto mat = std::make_shared<Material>("textured", ShadingModel::Lit);
    mat->SetTexture(TextureSlot::Diffuse, std::make_shared<FakeTexture>("in memory"));
    Entity c = scene.CreateEntity("textured");
    { MeshRendererComponent r; r.mesh = cube; r.materials = { mat }; c.Add<MeshRendererComponent>(r); }
    // an animator whose skeleton belongs to no known model
    Entity d = scene.CreateEntity("orphan animator");
    { AnimatorComponent an; an.skeleton = TwoBoneSkeleton(); d.Add<AnimatorComponent>(an); }

    const auto saved = SceneSerializer::Save(scene, bridge);
    CHECK(saved.warnings.size() == 4);
    CHECK(HasWarning(saved.warnings, "nameless mesh") && HasWarning(saved.warnings, "no mesh") && HasWarning(saved.warnings, "Diffuse texture") && HasWarning(saved.warnings, "orphan animator"));

    Scene loaded;
    const auto r = Load(saved.text, loaded, bridge);
    CHECK(r.ok);
    CHECK(loaded.EntityCount() == 4);                                                // every entity is kept
    CHECK(!loaded.FindByUuid(scene.GetUuid(a)).Has<MeshRendererComponent>());        // the part that could not be named is not there
    CHECK(!loaded.FindByUuid(scene.GetUuid(d)).Has<AnimatorComponent>());
    CHECK(loaded.FindByUuid(scene.GetUuid(c)).Has<MeshRendererComponent>());
}

static void TestLoadFailsLeavesSceneAlone()
{
    FakeBridge bridge;
    Scene scene;
    Entity keep = scene.CreateEntity("keep");
    const Uuid keepId = scene.GetUuid(keep);

    const struct { const char* text; const char* part; } cases[] = {
        { "", "not a valid scene file" },
        { "{ this is not json", "1:" },
        { "[1,2,3]", "not a DXEngine scene file" },
        { "{\"format\":\"something.else\",\"version\":1,\"entities\":[]}", "not a DXEngine scene file" },
        { "{\"format\":\"dxengine.scene\",\"entities\":[]}", "version" },
        { "{\"format\":\"dxengine.scene\",\"version\":0,\"entities\":[]}", "version" },
        { "{\"format\":\"dxengine.scene\",\"version\":999,\"entities\":[]}", "update the engine" },
        { "{\"format\":\"dxengine.scene\",\"version\":1}", "entities" },
        { "{\"format\":\"dxengine.scene\",\"version\":1,\"entities\":{}}", "entities" },
    };
    for (const auto& c : cases)
    {
        const auto r = Load(c.text, scene, bridge);
        CHECK(!r.ok);
        CHECK(r.error.find(c.part) != std::string::npos);
        CHECK(scene.EntityCount() == 1 && scene.FindByUuid(keepId).IsValid());       // untouched
    }
}

static void TestLoadRepairsWhatItCan()
{
    FakeBridge bridge;
    auto cube = bridge.AddMesh("primitive/cube");
    bridge.unloadable.insert("broken/mesh");
    bridge.meshes["broken/mesh"] = cube;

    // hand-written, with every kind of mistake
    const char* text = R"({
      "format": "dxengine.scene", "version": 1, "future": {"anything": [1,2,3]},
      "entities": [
        { "id": "00000000000000aa", "name": "child first", "parent": "00000000000000bb", "unknownKey": 5,
          "transform": { "position": [1, 2], "rotation": [0, 0, 0, 0], "scale": "big" } },
        { "id": "00000000000000bb", "name": "parent listed later",
          "transform": { "position": [3, 4, 5] } },
        { "id": "00000000000000bb", "name": "duplicate id" },
        { "id": "not an id", "name": "bad id" },
        { "name": "no id", "parent": "00000000000000ff" },
        { "id": "00000000000000cc", "name": "cycle a", "parent": "00000000000000dd" },
        { "id": "00000000000000dd", "name": "cycle b", "parent": "00000000000000cc" },
        { "id": "00000000000000ee", "name": "bad things",
          "light": { "type": "laser", "color": [1,2], "intensity": "bright" },
          "meshRenderer": { "mesh": "broken/mesh" } },
        { "id": "00000000000000e1", "name": "bad material",
          "meshRenderer": { "mesh": "primitive/cube", "materials": [ {"ref": "nope"}, {"material": 7}, null ] } },
        42,
        { "id": "00000000000000e2", "name": "animator without model", "animator": { "model": "missing.fbx" } }
      ]
    })";

    Scene scene;
    const auto r = Load(text, scene, bridge);
    CHECK(r.ok);
    CHECK(r.entities == 10);                                                        // the number is skipped, everything else loads
    CHECK(HasWarning(r.warnings, "is not an object") && HasWarning(r.warnings, "repeats the id") && HasWarning(r.warnings, "no valid id"));
    CHECK(HasWarning(r.warnings, "position is not 3 numbers") && HasWarning(r.warnings, "scale is not 3 numbers") && HasWarning(r.warnings, "zero quaternion"));
    CHECK(HasWarning(r.warnings, "not in the file") && HasWarning(r.warnings, "cycle"));
    CHECK(HasWarning(r.warnings, "unknown light type") && HasWarning(r.warnings, "light color") && HasWarning(r.warnings, "light 'intensity'"));
    CHECK(HasWarning(r.warnings, "broken/mesh") && HasWarning(r.warnings, "'nope'") && HasWarning(r.warnings, "material #7") && HasWarning(r.warnings, "missing.fbx"));

    Entity childFirst = scene.FindByUuid(Uuid{ 0xaa }), parentLater = scene.FindByUuid(Uuid{ 0xbb });
    CHECK(childFirst.IsValid() && parentLater.IsValid() && scene.GetParent(childFirst) == parentLater);   // a later parent is found
    CHECK(parentLater.Get<NameComponent>().name == "parent listed later");          // the first holder of an id keeps it
    const TransformComponent& t = childFirst.Get<TransformComponent>();
    CHECK(t.position.x == 0 && t.scale.x == 1 && t.rotation.w == 1);                // defaults kept
    CHECK(parentLater.Get<TransformComponent>().position.z == 5);

    Entity cycleA = scene.FindByUuid(Uuid{ 0xcc }), cycleB = scene.FindByUuid(Uuid{ 0xdd });
    CHECK(cycleA.IsValid() && cycleB.IsValid() && (!scene.GetParent(cycleA).IsValid() || !scene.GetParent(cycleB).IsValid()));   // never a loop

    Entity bad = scene.FindByUuid(Uuid{ 0xee });
    CHECK(bad.IsValid() && bad.Has<LightComponent>() && !bad.Has<MeshRendererComponent>());   // light kept (defaults), renderer dropped
    Entity badMat = scene.FindByUuid(Uuid{ 0xe1 });
    const MeshRendererComponent* bm = badMat.TryGet<MeshRendererComponent>();
    CHECK(bm && bm->materials.size() == 3 && !bm->materials[0] && !bm->materials[1] && !bm->materials[2]);   // all fall back to the mesh's own
    CHECK(!scene.FindByUuid(Uuid{ 0xe2 }).Has<AnimatorComponent>());

    // every entity has a distinct, valid id and can be found by it
    size_t found = 0;
    scene.Registry().view<IdComponent>().each([&](const IdComponent& id) { if (scene.FindByUuid(id.id).IsValid()) ++found; });
    CHECK(found == scene.EntityCount());
}

static void TestMaterialTextureProblems()
{
    FakeBridge bridge;
    bridge.AddMesh("m");
    const char* text = R"({
      "format": "dxengine.scene", "version": 1,
      "materials": [
        { "name": "odd", "shading": "Plasma", "diffuse": [1,2,3], "roughness": "rough",
          "state": { "blend": "Frosted", "cull": "Front" },
          "textures": { "Diffuse": "gone.png", "Sparkle": "x.png" } },
        7
      ],
      "entities": [ { "id": "0000000000000001", "name": "e", "meshRenderer": { "mesh": "m", "materials": [ {"material": 0}, {"material": 1} ] } } ]
    })";
    Scene scene;
    const auto r = Load(text, scene, bridge);
    CHECK(r.ok);
    CHECK(HasWarning(r.warnings, "unknown shading model") && HasWarning(r.warnings, "'diffuse'") && HasWarning(r.warnings, "'roughness'"));
    CHECK(HasWarning(r.warnings, "unknown blend") && HasWarning(r.warnings, "gone.png") && HasWarning(r.warnings, "Sparkle") && HasWarning(r.warnings, "material #1 is not an object"));
    const MeshRendererComponent* mr = scene.FindByUuid(Uuid{ 1 }).TryGet<MeshRendererComponent>();
    CHECK(mr && mr->materials.size() == 2 && mr->materials[0] && mr->materials[1]);
    CHECK(mr->materials[0]->GetShadingModel() == ShadingModel::Lit && mr->materials[0]->GetRenderState().cull == RHI::CullMode::Front);
    CHECK(mr->materials[0]->GetRenderState().blend == RHI::BlendMode::Opaque && mr->materials[0]->GetTextureMask() == 0);
}

static void TestLargeScene()
{
    FakeBridge bridge;
    auto cube = bridge.AddMesh("primitive/cube");
    auto mat = std::make_shared<Material>("shared", ShadingModel::Lit);
    Scene scene;
    Entity root = scene.CreateEntity("root");
    for (int i = 0; i < 10000; ++i)
    {
        Entity e = scene.CreateEntity("e" + std::to_string(i), root);
        e.Get<TransformComponent>().position = { float(i), 0.5f * i, -float(i) };
        MeshRendererComponent r; r.mesh = cube; r.materials = { mat };
        e.Add<MeshRendererComponent>(r);
    }
    const auto t0 = std::chrono::steady_clock::now();
    const auto saved = SceneSerializer::Save(scene, bridge);
    const auto t1 = std::chrono::steady_clock::now();
    Scene loaded;
    const auto r = Load(saved.text, loaded, bridge);
    const auto t2 = std::chrono::steady_clock::now();
    const double saveMs = std::chrono::duration<double, std::milli>(t1 - t0).count(), loadMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
    std::printf("  (10001 entities: save %.1f ms, load %.1f ms, %zu KB of text)\n", saveMs, loadMs, saved.text.size() / 1024);
    CHECK(r.ok && r.warnings.empty() && loaded.EntityCount() == 10001);
    CHECK(loaded.GetChildren(loaded.FindByUuid(scene.GetUuid(root))).size() == 10000);
    CHECK(SceneSerializer::Save(loaded, bridge).text == saved.text);
    CHECK(bridge.meshResolves >= 1 && bridge.meshResolves <= 2);                    // the cache: one resolve for 10000 users
    CHECK(saveMs < 2000.0 && loadMs < 2000.0);                                      // generous: a quadratic loop would take minutes
}

int main()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "empty scene, loading replaces",               TestEmptyScene },
        { "hierarchy, ids, sibling order, transforms",   TestHierarchyAndTransforms },
        { "components, sharing, inline materials",       TestComponents },
        { "animator: model, settings, no bone matrices", TestAnimator },
        { "saving warns about what has no name",         TestSaveWarnings },
        { "a bad file changes nothing",                  TestLoadFailsLeavesSceneAlone },
        { "loading repairs what it can, says so",        TestLoadRepairsWhatItCan },
        { "material and texture problems",               TestMaterialTextureProblems },
        { "10000 entities",                              TestLargeScene },
    };
    for (const Test& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll scene serializer tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
